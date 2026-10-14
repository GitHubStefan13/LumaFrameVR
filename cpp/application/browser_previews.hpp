#pragma once
#include "browser.hpp"
#include "thumbnail_services.hpp"
#include "image_loader.hpp"
#include <utility>
namespace lumaframevr::application {
struct PreviewDetails {
    std::filesystem::path path;
    int width{}, height{};
    std::optional<double> duration;
};
struct BrowserPreviewUpdate {
    bool changed{};
    std::optional<std::string> error;
};
class BrowserPreviews {
   public:
    explicit BrowserPreviews(std::filesystem::path directory) : browser(std::move(directory)) {}
    TileWanted wanted(bool playing) const;
    BrowserPreviewUpdate update(bool playing, bool dragging);
    const TileWanted& visible() const { return visible_; }
    RetainedFrame take_preview() { return std::move(preview_); }
    std::optional<ImagePixels> take_image_preview() { return std::exchange(image_preview_, std::nullopt); }
    FolderBrowser browser;
    ui::BrowserMode view{ui::BrowserMode::List};
    ThumbnailCache tiles;
    std::optional<PreviewDetails> preview_details;

   private:
    TileWanted visible_;
    PreviewFetcher fetch_;
    ImageLoader image_fetch_;
    std::optional<ImagePixels> image_preview_;
    std::optional<std::filesystem::path> image_pending_;
    RetainedFrame preview_;
    std::optional<std::filesystem::path> preview_path_;
};
}  // namespace lumaframevr::application
