#include "decode_worker_state.hpp"
#include <algorithm>
#include <cmath>

namespace lumaframevr {
DecodeWorker::DecodeWorker(MediaSource source, bool software, std::optional<std::pair<float, bool>> initial_volume) : shared_(std::make_shared<Shared>()) {
    if(initial_volume)
        shared_->volume = std::pair(std::clamp(initial_volume->first, 0.0F, 1.0F), initial_volume->second);
    thread_ = std::thread([source = std::move(source), software, shared = shared_] {
        try {
            decode(source, software, shared);
        } catch(...) {
            std::lock_guard lock(shared->mutex);
            shared->error = std::current_exception();
            shared->changed.notify_all();
        }
    });
}
DecodeWorker::~DecodeWorker() {
    stop();
}
void DecodeWorker::stop() {
    std::deque<DecodeResult> retired;
    {
        std::lock_guard lock(shared_->mutex);
        shared_->stopped = true;
        retired.swap(shared_->frames);
        shared_->status.queue_depth = 0;
    }
    shared_->changed.notify_all();
    if(thread_.joinable())
        thread_.join();
}
WorkerSnapshot DecodeWorker::snapshot() const {
    std::lock_guard lock(shared_->mutex);
    if(shared_->error)
        std::rethrow_exception(shared_->error);
    return shared_->status;
}
DecodeResult DecodeWorker::next() {
    std::lock_guard lock(shared_->mutex);
    if(shared_->error)
        std::rethrow_exception(shared_->error);
    if(shared_->frames.empty())
        return {DecodeStatus::pending, {}};
    auto result = std::move(shared_->frames.front());
    shared_->frames.pop_front();
    shared_->status.queue_depth = shared_->frames.size();
    shared_->changed.notify_one();
    return result;
}
std::uint64_t DecodeWorker::seek(double target) {
    if(!std::isfinite(target) || target < 0)
        throw std::invalid_argument("Invalid seek target");
    std::deque<DecodeResult> retired;
    std::lock_guard lock(shared_->mutex);
    auto& status = shared_->status;
    ++status.generation;
    retired.swap(shared_->frames);
    status.queue_depth = 0;
    status.release_required = true;
    status.ready = false;
    status.audio.clock = status.audio.clock ? std::optional(target) : std::nullopt;
    status.audio.limit = target;
    status.audio.done = false;
    shared_->audio_observed.reset();
    shared_->seek = target;
    shared_->running = false;
    shared_->changed.notify_one();
    return status.generation;
}
void DecodeWorker::released(std::uint64_t generation) {
    std::lock_guard lock(shared_->mutex);
    if(shared_->status.generation == generation)
        shared_->status.release_required = false;
    shared_->changed.notify_one();
}
void DecodeWorker::pause(bool paused) {
    std::lock_guard lock(shared_->mutex);
    shared_->status.paused = paused;
    shared_->changed.notify_one();
}
void DecodeWorker::begin() {
    std::lock_guard lock(shared_->mutex);
    shared_->running = true;
    shared_->changed.notify_one();
}
void DecodeWorker::preview() {
    std::lock_guard lock(shared_->mutex);
    shared_->preview = true;
    shared_->changed.notify_one();
}
void DecodeWorker::volume(float level, bool muted) {
    if(!std::isfinite(level))
        throw std::invalid_argument("Invalid volume");
    std::lock_guard lock(shared_->mutex);
    shared_->volume = std::pair(std::clamp(level, 0.0F, 1.0F), muted);
    shared_->changed.notify_one();
}
std::optional<double> DecodeWorker::audio_clock() const {
    std::lock_guard lock(shared_->mutex);
    const auto elapsed = shared_->status.paused || !shared_->audio_observed ? 0.0 : std::chrono::duration<double>(std::chrono::steady_clock::now() - *shared_->audio_observed).count();
    return shared_->status.audio.position(elapsed);
}
}  // namespace lumaframevr
