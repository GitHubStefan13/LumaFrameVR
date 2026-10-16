#include "browser_previews.hpp"
#include "catalogue.hpp"
#include "image_reader.hpp"
#include "image_thumbnail.hpp"
#include "preview_layout.hpp"
#include <algorithm>
namespace lumaframevr::application {
TileWanted BrowserPreviews::wanted(bool playing) const {
    TileWanted wanted;
    if(playing)
        return wanted;
    wanted.width = view == ui::BrowserMode::Grid ? 170 : 48;
    const auto viewport = ui::browser_viewport(ui::browser_listing_width, 800, browser.entries().size(), browser.selected(), view);
    for(std::size_t index = viewport.first; index < viewport.first + viewport.count; ++index) {
        const auto& entry = browser.entries()[index];
        if(!entry.directory && (ui::is_video_path(entry.path) || is_image_path(entry.path)))
            wanted.entries.emplace_back(index, entry.path);
    }
    return wanted;
}
BrowserPreviewUpdate BrowserPreviews::update(bool playing, bool dragging) {
    BrowserPreviewUpdate update;
    visible_ = wanted(playing);
    tiles.want(visible_);
    const auto previous_revision = tiles.revision();
    tiles.update();
    update.changed = tiles.revision() != previous_revision;
    if(playing) {
        fetch_.clear();
        image_fetch_.clear();
        image_preview_.reset();
        image_pending_.reset();
        preview_ = {};
        preview_path_.reset();
        preview_details.reset();
        return update;
    }
    if(dragging)
        return update;
    const auto selected = browser.selected_path();
    if(std::filesystem::is_regular_file(selected) && is_image_path(selected)) {
        fetch_.clear();
        preview_ = {};
        if(image_pending_ != selected) {
            image_fetch_.request(selected);
            image_pending_ = selected;
            image_preview_.reset();
            preview_details.reset();
            preview_path_.reset();
        }
        if(auto result = image_fetch_.take(); result && result->path == selected) {
            preview_path_ = selected;
            if(result->pixels) {
                PreviewDetails details;
                details.path = selected;
                details.width = result->pixels->width;
                details.height = result->pixels->height;
                preview_details = std::move(details);
                image_preview_ = thumbnail_image(*result->pixels, preview_extent[0]);
            } else
                update.error = i18n::tr("error.explorer_image", {{"detail", result->error}});
            update.changed = true;
        }
        return update;
    }
    image_fetch_.clear();
    image_pending_.reset();
    image_preview_.reset();
    if(!std::filesystem::is_regular_file(selected) || !ui::is_video_path(selected)) {
        fetch_.clear();
        preview_path_.reset();
        preview_details.reset();
        preview_ = {};
        return update;
    }
    const ThumbnailRequest request{selected, 10, std::nullopt};
    if(fetch_.pending() != request && preview_path_ != selected)
        fetch_.request(request);
    if(auto result = fetch_.poll(); result && result->request.path == selected) {
        preview_path_ = selected;
        if(result->preview) {
            const auto& decoder = result->preview->decoder;
            std::uintmax_t size = 0;
            for(const auto& entry : browser.entries())
                if(entry.path == selected) {
                    size = entry.size;
                    break;
                }
            PreviewDetails details;
            details.path = selected;
            details.width = result->preview->width;
            details.height = result->preview->height;
            details.duration = result->preview->duration;
            details.codec = decoder.name;
            details.bits = decoder.bits;
            details.fps = decoder.fps;
            details.bitrate_mbps = decoder.bitrate_mbps;
            details.size = size;
            preview_details = std::move(details);
            preview_ = std::move(result->preview->frame);
        } else {
            preview_details.reset();
            update.error = i18n::tr("error.explorer_preview", {{"detail", result->error}});
        }
        update.changed = true;
    }
    return update;
}
}  // namespace lumaframevr::application
