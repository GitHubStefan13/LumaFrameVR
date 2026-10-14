#pragma once
#include "image_loader.hpp"
#include <array>
namespace lumaframevr::application {
class ImageViewing {
   public:
    void open(const std::filesystem::path& path);
    void stop();
    bool update();
    std::optional<ImagePixels> take();
    bool active() const { return active_; }
    bool loading() const { return loading_; }
    bool release_required() const { return release_required_; }
    void released() { release_required_ = false; }
    std::array<int, 2> dimensions() const { return dimensions_; }
    const std::string& error() const { return error_; }

   private:
    ImageLoader loader_;
    std::filesystem::path path_;
    std::optional<ImagePixels> pixels_;
    std::array<int, 2> dimensions_{};
    std::string error_;
    bool active_{}, loading_{}, release_required_{};
};
}  // namespace lumaframevr::application
