#include "image_conversion.hpp"
#include <algorithm>
#include <cmath>
extern "C" {
#include <libavutil/frame.h>
}
namespace lumaframevr {
ImagePixels orient_image(ImagePixels image, const AVFrame& frame) {
    const auto* data = av_frame_get_side_data(&frame, AV_FRAME_DATA_DISPLAYMATRIX);
    if(!data || data->size < sizeof(std::int32_t) * 9)
        return image;
    const auto* matrix = reinterpret_cast<const std::int32_t*>(data->data);
    const auto sign = [](std::int32_t value) { return (value > 0) - (value < 0); };
    const int a = sign(matrix[0]), b = sign(matrix[1]), c = sign(matrix[3]), d = sign(matrix[4]);
    if(std::abs(a) + std::abs(b) != 1 || std::abs(c) + std::abs(d) != 1 || a * c + b * d != 0)
        return image;
    if(a == 1 && d == 1)
        return image;
    const int left = std::min(0, (image.width - 1) * a) + std::min(0, (image.height - 1) * c);
    const int top = std::min(0, (image.width - 1) * b) + std::min(0, (image.height - 1) * d);
    ImagePixels output{std::abs(a) * image.width + std::abs(c) * image.height, std::abs(b) * image.width + std::abs(d) * image.height, std::vector<std::uint8_t>(image.rgba.size())};
    for(int y = 0; y < image.height; ++y)
        for(int x = 0; x < image.width; ++x) {
            const auto from = (static_cast<std::size_t>(y) * image.width + x) * 4;
            const auto to = (static_cast<std::size_t>(x * b + y * d - top) * output.width + x * a + y * c - left) * 4;
            std::copy_n(image.rgba.begin() + from, 4, output.rgba.begin() + to);
        }
    return output;
}
}  // namespace lumaframevr
