#pragma once
#include "advanced_panels.hpp"
namespace lumaframevr::ui {
std::string advanced_fit(std::string_view text, std::size_t columns);
void advanced_header(Canvas& canvas, std::string_view title, std::string_view trailing);
void advanced_line(Canvas& canvas, Rect area, std::string_view text, bool selected, Color color);
void advanced_draw_slider(Canvas& canvas, Rect area, std::string_view text, float fraction);
}  // namespace lumaframevr::ui
