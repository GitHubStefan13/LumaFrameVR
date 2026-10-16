#include "browser.hpp"
#include "browser_colors.hpp"
#include "catalogue.hpp"
#include "font_style.hpp"
#include "icons.hpp"
#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>

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
std::string file_size_text(std::uintmax_t bytes) {
    static constexpr std::array<std::string_view, 5> units{"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while(value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream out;
    if(unit == 0)
        out << bytes << " B";
    else
        out << std::fixed << std::setprecision(1) << value << ' ' << units[unit];
    return out.str();
}
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
            // A size, when asked for, gets its own right-aligned column and the name gets what is
            // left: a clipped name beside a size is fine, a name drawn under a size is not.
            const auto size_text = state.show_file_sizes && !entry.directory && entry.size ? file_size_text(entry.size) : std::string{};
            const auto size_width = size_text.empty() ? 0 : text_width(size_text, FontStyle::secondary) + 20;
            const auto columns = static_cast<std::size_t>(std::max(1, (row.right() - 40 - size_width - left) / 9));
            draw_text(canvas, left, row.y + (row.height - text_height(FontStyle::body)) / 2, fit_label(entry_label(entry, browser.directory()), columns), active ? Color{255, 255, 255, 255} : text,
                      FontStyle::body);
            if(!size_text.empty())
                draw_text(canvas, row.right() - 24 - text_width(size_text, FontStyle::secondary), row.y + (row.height - text_height(FontStyle::secondary)) / 2, size_text,
                          active ? Color{255, 255, 255, 255} : dim, FontStyle::secondary);
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
