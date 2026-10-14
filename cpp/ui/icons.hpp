#pragma once
#include "canvas.hpp"
#include <string_view>

namespace lumaframevr::ui {
// Draws a baked Lucide icon by name into an area. The names are the ones scripts/make-icon-atlas.py
// writes; an unknown name draws nothing rather than failing a frame.
void draw_icon(Canvas& canvas, Rect area, std::string_view name, Color color);
}  // namespace lumaframevr::ui
