#pragma once
#include "canvas.hpp"
namespace lumaframevr::ui {
inline constexpr int branding_header_height = 120;
void draw_branding(Canvas& canvas, Rect bounds, bool compact);
}  // namespace lumaframevr::ui
