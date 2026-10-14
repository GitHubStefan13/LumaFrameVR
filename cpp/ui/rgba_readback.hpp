#pragma once
#include "canvas.hpp"
#include <cstddef>
namespace lumaframevr::ui {
Canvas canvas_from_readback(int width, int height, std::span<const std::byte> pixels, bool bgra = false);
}
