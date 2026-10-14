#include "font_style.hpp"
#include "assets/font_body.hpp"
#include "assets/font_medium.hpp"
#include "assets/font_secondary.hpp"
#include "assets/font_title.hpp"
#include <array>

namespace lumaframevr::ui {
namespace {
using Style = FontStyle;
struct Atlas {
    const fonts::Glyph* glyphs;
    const std::uint8_t* pixels;
    const char32_t* extras;
    int extras_count, first, last, size, ascent, line_height;
};
constexpr Atlas atlases[] = {
        {fonts::title_glyphs.data(), fonts::title_pixels.data(), fonts::title_extras.data(), static_cast<int>(fonts::title_extras.size()), fonts::title_first, fonts::title_last, fonts::title_size,
         fonts::title_ascent, fonts::title_line_height},
        {fonts::body_glyphs.data(), fonts::body_pixels.data(), fonts::body_extras.data(), static_cast<int>(fonts::body_extras.size()), fonts::body_first, fonts::body_last, fonts::body_size,
         fonts::body_ascent, fonts::body_line_height},
        {fonts::medium_glyphs.data(), fonts::medium_pixels.data(), fonts::medium_extras.data(), static_cast<int>(fonts::medium_extras.size()), fonts::medium_first, fonts::medium_last,
         fonts::medium_size, fonts::medium_ascent, fonts::medium_line_height},
        {fonts::secondary_glyphs.data(), fonts::secondary_pixels.data(), fonts::secondary_extras.data(), static_cast<int>(fonts::secondary_extras.size()), fonts::secondary_first,
         fonts::secondary_last, fonts::secondary_size, fonts::secondary_ascent, fonts::secondary_line_height},
};
const Atlas& atlas(Style style) {
    return atlases[static_cast<std::size_t>(style)];
}
// ASCII is a direct index; everything else is the short table the bake appends after it. A
// character the interface never uses has no glyph and simply does not advance.
const fonts::Glyph* glyph(const Atlas& font, char32_t character) {
    if(character >= static_cast<char32_t>(font.first) && character <= static_cast<char32_t>(font.last))
        return &font.glyphs[character - font.first];
    for(int index = 0; index < font.extras_count; ++index)
        if(font.extras[index] == character)
            return &font.glyphs[font.last - font.first + 1 + index];
    return nullptr;
}
}  // namespace
int text_height(Style style) {
    return atlas(style).line_height;
}
int text_ascent(Style style) {
    return atlas(style).ascent;
}
int text_width(std::string_view text, Style style) {
    const auto& font = atlas(style);
    int width = 0;
    for(const auto character : decode_utf8(text))
        if(const auto* found = glyph(font, character))
            width += found->advance;
    return width;
}
int control_width(std::string_view label, FontStyle style, bool icon, int padding) {
    return padding + (icon ? 30 : 0) + text_width(label, style);
}
std::vector<std::string> wrap_text(std::string_view text, int width, Style style) {
    std::vector<std::string> lines;
    std::string line;
    std::size_t start = 0;
    while(start <= text.size()) {
        const auto end = text.find(' ', start);
        const auto word = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        const auto candidate = line.empty() ? std::string(word) : line + " " + std::string(word);
        if(!line.empty() && text_width(candidate, style) > width) {
            lines.push_back(line);
            line = std::string(word);
        } else {
            line = candidate;
        }
        if(end == std::string_view::npos)
            break;
        start = end + 1;
    }
    if(!line.empty())
        lines.push_back(line);
    return lines;
}

void draw_text(Canvas& canvas, int x, int y, std::string_view text, Color color, Style style) {
    const auto& font = atlas(style);
    int pen = x;
    for(const auto character : decode_utf8(text)) {
        const auto* found = glyph(font, character);
        if(!found)
            continue;
        if(found->width && found->height)
            canvas.blend(pen + found->left, y + found->top, {font.pixels + found->offset, static_cast<std::size_t>(found->width) * found->height}, found->width, found->height, color);
        pen += found->advance;
    }
}
}  // namespace lumaframevr::ui
