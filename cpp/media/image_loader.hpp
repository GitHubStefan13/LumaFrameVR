#pragma once
#include "image_reader.hpp"
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
namespace lumaframevr {
struct ImageResult {
    std::filesystem::path path;
    std::optional<ImagePixels> pixels;
    std::string error;
};
class ImageLoader {
   public:
    using Reader = std::function<ImagePixels(const std::filesystem::path&, const std::function<bool()>&)>;
    explicit ImageLoader(Reader reader = read_image);
    ~ImageLoader();
    ImageLoader(const ImageLoader&) = delete;
    ImageLoader& operator=(const ImageLoader&) = delete;
    void request(const std::filesystem::path& path);
    void clear();
    std::optional<ImageResult> take();

   private:
    void run();
    Reader reader_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::optional<std::filesystem::path> wanted_;
    std::optional<ImageResult> result_;
    std::uint64_t generation_{};
    bool stopping_{};
    std::thread worker_;
};
}  // namespace lumaframevr
