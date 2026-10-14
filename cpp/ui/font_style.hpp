#pragma once
#include "canvas.hpp"
#include <string>
#include <vector>
#include <string_view>

namespace lumaframevr::ui {
// The interface typeface, baked from Inter (see scripts/make-font-atlas.py). A style names a
// size and weight rather than a font: the title of a dialog, a file name, a button or sidebar
// row, and secondary text.
enum class FontStyle { title, body, medium, secondary };

int text_height(FontStyle style);
int text_ascent(FontStyle style);
// The width the string occupies, from the advances baked with the glyphs. Proportional, so this
// is the only way the layout can measure text.
int text_width(std::string_view text, FontStyle style);
// A control sized to its content. Translated text is never a fixed width, so a labelled control
// has to be measured from the label it is about to draw; a constant here is a bug the next
// language will find. The padding covers the left and right inset, the icon carries its own.
int control_width(std::string_view label, FontStyle style, bool icon = false, int padding = 28);

// Draws with y at the top of the line box, which is what the layouts reason about.
void draw_text(Canvas& canvas, int x, int y, std::string_view text, Color color, FontStyle style);

// Breaks prose into lines that fit a width, on word boundaries. The panels draw labels, which never
// need this; the notice and the licence list are sentences, and those do.
std::vector<std::string> wrap_text(std::string_view text, int width, FontStyle style);
}  // namespace lumaframevr::ui
