#include "canvas.hpp"
#include <algorithm>
#include <cmath>

namespace lumaframevr::ui {
void Canvas::image(Rect area, std::span<const std::uint8_t> rgba, int width, int height, std::uint8_t opacity) {
    if(width <= 0 || height <= 0 || area.width <= 0 || area.height <= 0)
        return;
    if(rgba.size() < static_cast<std::size_t>(width) * height * 4)
        return;
    const auto scale = std::min(static_cast<float>(area.width) / width, static_cast<float>(area.height) / height);
    const int drawn_width = std::max(1, static_cast<int>(std::round(width * scale)));
    const int drawn_height = std::max(1, static_cast<int>(std::round(height * scale)));
    const int origin_x = area.x + (area.width - drawn_width) / 2;
    const int origin_y = area.y + (area.height - drawn_height) / 2;
    for(int y = 0; y < drawn_height; ++y)
        for(int x = 0; x < drawn_width; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * height / drawn_height * width + static_cast<std::size_t>(x) * width / drawn_width) * 4;
            Color color{rgba[offset], rgba[offset + 1], rgba[offset + 2], rgba[offset + 3]};
            color[3] = static_cast<std::uint8_t>(static_cast<unsigned>(color[3]) * opacity / 255);
            put(origin_x + x, origin_y + y, color);
        }
}
void Canvas::glyph(Rect area, std::span<const std::uint8_t> coverage, int source_width, int source_height, Color color) {
    if(source_width <= 0 || source_height <= 0)
        return;
    if(static_cast<int>(coverage.size()) < source_width * source_height)
        return;
    if(area.width <= 0 || area.height <= 0)
        return;
    for(int y = 0; y < area.height; ++y) {
        const auto source_y = static_cast<int>((static_cast<long long>(y) * source_height) / area.height);
        for(int x = 0; x < area.width; ++x) {
            const auto source_x = static_cast<int>((static_cast<long long>(x) * source_width) / area.width);
            const auto alpha = static_cast<unsigned>(coverage[static_cast<std::size_t>(source_y) * source_width + source_x]) * color[3] / 255;
            const auto px = area.x + x, py = area.y + y;
            if(!alpha || px < 0 || py < 0 || px >= width() || py >= height())
                continue;
            const auto existing = pixel(px, py);
            const auto background = static_cast<unsigned>(existing[3]) * (255 - alpha) / 255;
            Color blended{};
            for(int channel = 0; channel < 3; ++channel) blended[channel] = static_cast<std::uint8_t>((color[channel] * alpha + existing[channel] * background) / (alpha + background));
            blended[3] = static_cast<std::uint8_t>(alpha + background);
            put(px, py, blended);
        }
    }
}

void Canvas::glyph(Rect area, std::span<const std::uint8_t, 1600> coverage, Color color) {
    // The 40 pixel assets are drawn at their own size, centred, as they always were.
    glyph({area.x + (area.width - 40) / 2, area.y + (area.height - 40) / 2, 40, 40}, std::span<const std::uint8_t>(coverage.data(), coverage.size()), 40, 40, color);
}
void Canvas::blend(int x, int y, std::span<const std::uint8_t> coverage, int width, int height, Color color) {
    if(static_cast<int>(coverage.size()) < width * height)
        return;
    for(int row = 0; row < height; ++row)
        for(int column = 0; column < width; ++column) {
            const auto alpha = coverage[static_cast<std::size_t>(row) * width + column];
            if(!alpha)
                continue;
            const auto px = x + column, py = y + row;
            if(px < 0 || py < 0 || px >= width_ || py >= height_)
                continue;
            const auto existing = pixel(px, py);
            const auto background = static_cast<unsigned>(existing[3]) * (255 - alpha) / 255;
            Color blended{};
            for(int channel = 0; channel < 3; ++channel) blended[channel] = static_cast<std::uint8_t>((color[channel] * alpha + existing[channel] * background) / (alpha + background));
            blended[3] = static_cast<std::uint8_t>(alpha + background);
            put(px, py, blended);
        }
}

}  // namespace lumaframevr::ui
