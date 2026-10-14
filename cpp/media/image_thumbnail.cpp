#include "image_thumbnail.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace lumaframevr {
ImagePixels thumbnail_image(const ImagePixels& image, unsigned requested_width) {
    if(!requested_width || requested_width > 512)
        throw std::invalid_argument("Thumbnail width must be within 1..512");
    if(image.width <= 0 || image.height <= 0 || static_cast<std::int64_t>(image.width) * image.height > maximum_image_pixels ||
       image.rgba.size() != static_cast<std::size_t>(image.width) * image.height * 4)
        throw std::invalid_argument("Invalid image thumbnail source");
    const double scale = std::min({1.0, static_cast<double>(requested_width) / image.width, static_cast<double>(requested_width) * 2 / image.height});
    ImagePixels output{std::max(1, static_cast<int>(std::round(image.width * scale))), std::max(1, static_cast<int>(std::round(image.height * scale))), {}};
    output.rgba.resize(static_cast<std::size_t>(output.width) * output.height * 4);
    for(int y = 0; y < output.height; ++y)
        for(int x = 0; x < output.width; ++x) {
            const auto source = (static_cast<std::size_t>(y) * image.height / output.height * image.width + static_cast<std::size_t>(x) * image.width / output.width) * 4;
            const auto destination = (static_cast<std::size_t>(y) * output.width + x) * 4;
            std::copy_n(image.rgba.begin() + source, 4, output.rgba.begin() + destination);
        }
    return output;
}
}  // namespace lumaframevr
