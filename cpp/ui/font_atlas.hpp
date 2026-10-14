#pragma once
#include <cstdint>

namespace lumaframevr::ui::fonts {
// One baked glyph: where its ink starts in the atlas, how far the pen moves, and where the ink
// sits relative to the pen and the top of the line box. left and top are signed because a glyph
// can lean outside both.
struct Glyph {
    std::uint16_t offset;
    std::uint8_t advance, width, height;
    std::int8_t left, top;
};
}  // namespace lumaframevr::ui::fonts
