#include "icons.hpp"
#include "assets/icons.hpp"
#include <span>

namespace lumaframevr::ui {
void draw_icon(Canvas& canvas, Rect area, std::string_view name, Color color) {
    for(const auto& entry : assets::icons)
        if(std::string_view(entry.name) == name)
            canvas.glyph(area, {entry.coverage, static_cast<std::size_t>(entry.width) * entry.height}, entry.width, entry.height, color);
}
}  // namespace lumaframevr::ui
