#include "browser.hpp"
#include "browser_colors.hpp"
#include "catalogue.hpp"
#include "font_style.hpp"
#include "icons.hpp"
#include <algorithm>

namespace lumaframevr::ui {
namespace {
using namespace browser_color;
std::string entry_label(const FolderEntry& entry, const std::filesystem::path& directory) {
    auto name = entry.path == directory.parent_path() ? i18n::tr("browser.parent") : escape_path(entry.path.filename());
    if(entry.directory && name != i18n::tr("browser.parent"))
        name.push_back('/');
    return name;
}
const Thumbnail* thumbnail(std::span<const Thumbnail> thumbnails, std::size_t index) {
    for(const auto& tile : thumbnails)
        if(tile.index == index && !tile.pixels.empty())
            return &tile;
    return nullptr;
}
}  // namespace
void draw_browser_entries(Canvas& canvas, const FolderBrowser& browser, const BrowserRenderState& state) {
    const auto viewport = browser_viewport(canvas.width(), canvas.height(), browser.entries().size(), browser.selected(), state.mode);
    const auto& entries = browser.entries();
    if(state.mode == BrowserMode::List) {
        for(std::size_t slot = 0; slot < viewport.count; ++slot) {
            const auto index = viewport.first + slot;
            const auto& entry = entries[index];
            const auto row = viewport.rectangle(slot);
            const bool active = index == browser.selected();
            if(active)
                canvas.rounded(row, 10, selected);
            else if(state.hover == index)
                canvas.rounded(row, 10, highlight);
            const Rect icon{row.x + 16, row.y + (row.height - 26) / 2, 26, 26};
            if(const auto* tile = thumbnail(state.thumbnails, index)) {
                canvas.image(icon, tile->pixels, tile->width, tile->height);
            } else {
                draw_file_icon(canvas, icon, entry.directory, is_video_path(entry.path));
            }
            const auto left = icon.right() + 14;
            const auto columns = static_cast<std::size_t>(std::max(1, (row.right() - 40 - left) / 9));
            draw_text(canvas, left, row.y + (row.height - text_height(FontStyle::body)) / 2, fit_label(entry_label(entry, browser.directory()), columns), active ? Color{255, 255, 255, 255} : text,
                      FontStyle::body);
            // A folder leads somewhere, so it carries a chevron the way the reference does.
            if(entry.directory)
                draw_icon(canvas, {row.right() - 36, row.y + (row.height - 20) / 2, 20, 20}, "chevron_right", active ? Color{255, 255, 255, 255} : dim);
        }
        return;
    }
    for(std::size_t slot = 0; slot < viewport.count; ++slot) {
        const auto index = viewport.first + slot;
        const auto& entry = entries[index];
        const auto tile = viewport.rectangle(slot);
        const Rect picture{tile.x, tile.y, 176, 99};
        const bool active = index == browser.selected();
        if(active || state.hover == index)
            canvas.rounded(tile, 10, highlight);
        if(const auto* decoded = thumbnail(state.thumbnails, index))
            canvas.image(picture, decoded->pixels, decoded->width, decoded->height);
        else
            draw_file_icon(canvas, {picture.x + 56, picture.y + 8, 64, 56}, entry.directory, is_video_path(entry.path));
        if(active)
            canvas.outline(picture, 6, 2, accent);
        const auto shown = fit_label(entry_label(entry, browser.directory()), 20);
        draw_text(canvas, tile.x + std::max(0, (tile.width - text_width(shown, FontStyle::secondary)) / 2), tile.y + 101, shown, active ? Color{255, 255, 255, 255} : text, FontStyle::secondary);
    }
}
}  // namespace lumaframevr::ui
