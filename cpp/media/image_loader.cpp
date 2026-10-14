#include "image_loader.hpp"
#include "media_decoder_startup.hpp"
#include <utility>
namespace lumaframevr {
ImageLoader::ImageLoader(Reader reader) : reader_(std::move(reader)), worker_([this] { run(); }) {}
ImageLoader::~ImageLoader() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    ready_.notify_one();
    worker_.join();
}
void ImageLoader::request(const std::filesystem::path& path) {
    {
        std::lock_guard lock(mutex_);
        ++generation_;
        wanted_ = path;
        result_.reset();
    }
    ready_.notify_one();
}
void ImageLoader::clear() {
    std::lock_guard lock(mutex_);
    ++generation_;
    wanted_.reset();
    result_.reset();
}
std::optional<ImageResult> ImageLoader::take() {
    std::lock_guard lock(mutex_);
    return std::exchange(result_, std::nullopt);
}
void ImageLoader::run() {
    const media::PreviewDecode preview_decode;
    for(;;) {
        std::filesystem::path path;
        std::uint64_t generation;
        {
            std::unique_lock lock(mutex_);
            ready_.wait(lock, [this] { return stopping_ || wanted_.has_value(); });
            if(stopping_)
                return;
            path = std::move(*wanted_);
            wanted_.reset();
            generation = generation_;
        }
        ImageResult result{path, {}, {}};
        const auto cancelled = [this, generation] {
            std::lock_guard lock(mutex_);
            return stopping_ || generation != generation_;
        };
        try {
            result.pixels = reader_(path, cancelled);
        } catch(const std::exception& error) {
            result.error = error.what();
        } catch(...) {
            result.error = "Image decoder failed with an unknown exception";
        }
        {
            std::lock_guard lock(mutex_);
            if(!stopping_ && generation == generation_)
                result_ = std::move(result);
        }
    }
}
}  // namespace lumaframevr
