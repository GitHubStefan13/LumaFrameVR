#include "thumbnail_services.hpp"
#include "media_decoder_startup.hpp"
#include <condition_variable>
#include <cmath>
#include <mutex>
#include <thread>
namespace lumaframevr {
DecodedPreview decode_preview(const ThumbnailRequest& request) {
    if(request.seconds && (!std::isfinite(*request.seconds) || *request.seconds < 0))
        throw std::invalid_argument("Invalid absolute thumbnail target");
    DecoderSession decoder(MediaSource::parse(request.path.string()), true);
    const auto duration = decoder.duration();
    const auto seconds = request.seconds ? request.seconds : duration ? std::optional{*duration * request.position_percent / 100.0} : std::nullopt;
    if(seconds && *seconds > 0)
        decoder.seek(*seconds);
    auto frame = decoder.next_frame();
    if(!frame.valid())
        throw std::runtime_error("The file has no video frame: " + request.path.string());
    if(!frame.software())
        throw std::runtime_error("Thumbnail decoder unexpectedly returned a hardware frame");
    const auto width = frame.width(), height = frame.height();
    return {std::move(frame), width, height, duration, decoder.description()};
}
struct PreviewFetcher::State {
    mutable std::mutex mutex;
    std::condition_variable changed;
    bool stopped{};
    std::uint64_t generation{};
    std::optional<ThumbnailRequest> wanted, queued;
    std::optional<PreviewResult> ready;
    std::thread worker;
    void run() {
        const media::PreviewDecode preview_decode;
        for(;;) {
            ThumbnailRequest request;
            std::uint64_t serial;
            {
                std::unique_lock lock(mutex);
                changed.wait(lock, [&] { return stopped || queued.has_value(); });
                if(stopped)
                    return;
                request = std::move(*queued);
                queued.reset();
                serial = generation;
            }
            PreviewResult result{request, {}, {}, serial};
            try {
                result.preview = std::make_shared<DecodedPreview>(decode_preview(request));
            } catch(const std::exception& error) {
                result.error = error.what();
            }
            {
                std::lock_guard lock(mutex);
                if(!stopped && serial == generation && wanted == request)
                    ready = std::move(result);
            }
        }
    }
};
PreviewFetcher::PreviewFetcher() : state_(std::make_unique<State>()) {
    state_->worker = std::thread([this] { state_->run(); });
}
PreviewFetcher::~PreviewFetcher() {
    clear();
    {
        std::lock_guard lock(state_->mutex);
        state_->stopped = true;
    }
    state_->changed.notify_all();
    state_->worker.join();
}
std::uint64_t PreviewFetcher::request(ThumbnailRequest request) {
    std::optional<PreviewResult> discarded;
    std::uint64_t generation;
    {
        std::lock_guard lock(state_->mutex);
        if(state_->wanted == request)
            return state_->generation;
        state_->wanted = request;
        state_->queued = std::move(request);
        discarded = std::move(state_->ready);
        state_->ready.reset();
        generation = ++state_->generation;
    }
    state_->changed.notify_one();
    return generation;
}
void PreviewFetcher::clear() {
    std::optional<PreviewResult> discarded;
    {
        std::lock_guard lock(state_->mutex);
        ++state_->generation;
        state_->wanted.reset();
        state_->queued.reset();
        discarded = std::move(state_->ready);
        state_->ready.reset();
    }
}
std::optional<PreviewResult> PreviewFetcher::poll() {
    std::lock_guard lock(state_->mutex);
    auto result = std::move(state_->ready);
    state_->ready.reset();
    return result;
}
std::optional<ThumbnailRequest> PreviewFetcher::pending() const {
    std::lock_guard lock(state_->mutex);
    return state_->wanted;
}
}  // namespace lumaframevr
