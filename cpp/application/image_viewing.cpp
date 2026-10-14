#include "image_viewing.hpp"
#include <utility>
namespace lumaframevr::application {
void ImageViewing::open(const std::filesystem::path& path) {
    stop();
    path_ = path;
    active_ = loading_ = true;
    loader_.request(path);
}
void ImageViewing::stop() {
    release_required_ |= active_;
    loader_.clear();
    pixels_.reset();
    dimensions_ = {};
    error_.clear();
    active_ = loading_ = false;
}
bool ImageViewing::update() {
    // A stopped view consumes nothing: a result taken here would be discarded unpublished, and
    // a superseded request's result is never this view's to publish over the one being opened.
    if(!active_)
        return false;
    auto result = loader_.take();
    if(!result || result->path != path_)
        return false;
    loading_ = false;
    error_ = std::move(result->error);
    pixels_ = std::move(result->pixels);
    if(pixels_)
        dimensions_ = {pixels_->width, pixels_->height};
    return true;
}
std::optional<ImagePixels> ImageViewing::take() {
    return std::exchange(pixels_, std::nullopt);
}
}  // namespace lumaframevr::application
