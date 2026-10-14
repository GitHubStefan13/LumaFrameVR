#include "thumbnail_services.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace lumaframevr {
ThumbnailPixels thumbnail_pixels(const RetainedFrame& frame, unsigned target) {
    if(!frame.valid() || !frame.software() || frame.width() < 2 || frame.height() < 2)
        throw std::invalid_argument("Thumbnail needs a valid software 4:2:0 frame");
    const int format = frame.format();
    const bool planar = format == 0 || format == 12 || format == 62;
    const bool wide = format == 62 || format == 159;
    if(!planar && format != 23 && format != 159)
        throw std::invalid_argument("Unsupported thumbnail pixel format " + std::to_string(format));
    const int width = frame.width(), height = frame.height();
    const int out_width = target == 0 ? width : static_cast<int>(std::min(target, static_cast<unsigned>(width)));
    const int out_height = std::max(1, static_cast<int>(std::round(static_cast<float>(height) * out_width / width)));
    ThumbnailPixels output{{}, out_width, out_height};
    output.pixels.resize(static_cast<std::size_t>(out_width) * out_height * 4);
    float kr = 0.2126F, kb = 0.0722F;
    if(frame.colorspace() == 5 || frame.colorspace() == 6) {
        kr = 0.299F;
        kb = 0.114F;
    }
    if(frame.colorspace() == 9 || frame.colorspace() == 10) {
        kr = 0.2627F;
        kb = 0.0593F;
    }
    const auto kg = 1 - kr - kb;
    const bool full = frame.color_range() >= 2;
    const auto sample = [&](int plane, int x, int y) {
        const auto bytes = frame.plane_row(plane, y);
        const auto offset = static_cast<std::size_t>(x) * (wide ? 2 : 1);
        if(offset + (wide ? 1 : 0) >= bytes.size())
            throw std::runtime_error("Thumbnail plane stride is truncated");
        if(!wide)
            return static_cast<float>(bytes[offset]);
        const unsigned word = bytes[offset] | static_cast<unsigned>(bytes[offset + 1]) << 8;
        return static_cast<float>(format == 62 ? word : word >> 6);
    };
    for(int y = 0; y < out_height; ++y) {
        const auto sy = y * (height * 1000 / out_height) / 1000;
        for(int x = 0; x < out_width; ++x) {
            const auto sx = x * (width * 1000 / out_width) / 1000;
            float luma = sample(0, sx, sy);
            float cb = planar ? sample(1, sx / 2, sy / 2) : sample(1, sx / 2 * 2, sy / 2);
            float cr = planar ? sample(2, sx / 2, sy / 2) : sample(1, sx / 2 * 2 + 1, sy / 2);
            const float peak = wide ? 1023.0F : 255.0F, center = wide ? 512.0F : 128.0F;
            if(!full) {
                const auto scale = peak / 255;
                luma = (luma - 16 * scale) / (219 * scale) * peak;
                cb = (cb - center) / (224 * scale) * peak;
                cr = (cr - center) / (224 * scale) * peak;
            } else {
                cb -= center;
                cr -= center;
            }
            const std::array rgb{luma + 2 * (1 - kr) * cr, luma - 2 * (1 - kb) * kb / kg * cb - 2 * (1 - kr) * kr / kg * cr, luma + 2 * (1 - kb) * cb};
            const auto offset = (static_cast<std::size_t>(y) * out_width + x) * 4;
            for(std::size_t i = 0; i < 3; ++i) output.pixels[offset + i] = static_cast<unsigned char>(std::round(std::clamp(rgb[i], 0.0F, peak) / peak * 255));
            output.pixels[offset + 3] = 255;
        }
    }
    return output;
}
}  // namespace lumaframevr
