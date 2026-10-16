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
    // What decoding the preview frame learned about the file, shown beside the preview the way the
    // information panel shows it for what is playing: the decoder's name, the sample depth, the frame
    // rate and the bitrate. The size comes from the folder scan, so it costs no extra stat.
    std::string codec;
    int bits{};
    double fps{}, bitrate_mbps{};
    std::uintmax_t size{};
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
