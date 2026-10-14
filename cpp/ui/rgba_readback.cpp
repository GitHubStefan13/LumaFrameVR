#include "rgba_readback.hpp"
#include <stdexcept>
namespace lumaframevr::ui {
Canvas canvas_from_readback(int width, int height, std::span<const std::byte> pixels, bool bgra) {
    if(width <= 0 || height <= 0 || width > 16384 || height > 16384 || pixels.size() != static_cast<std::size_t>(width) * height * 4)
        throw std::invalid_argument("Invalid top-down RGBA readback dimensions");
    Canvas canvas(width, height);
    for(int y = 0; y < height; ++y)
        for(int x = 0; x < width; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * width + x) * 4;
            canvas.put(x, y,
                       {std::to_integer<std::uint8_t>(pixels[offset + (bgra ? 2 : 0)]), std::to_integer<std::uint8_t>(pixels[offset + 1]),
                        std::to_integer<std::uint8_t>(pixels[offset + (bgra ? 0 : 2)]), std::to_integer<std::uint8_t>(pixels[offset + 3])});
        }
    return canvas;
}
}  // namespace lumaframevr::ui
