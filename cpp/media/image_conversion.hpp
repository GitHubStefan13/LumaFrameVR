#pragma once
#include "image_reader.hpp"
struct AVFrame;
namespace lumaframevr {
ImagePixels image_rgba(const AVFrame& frame, const std::function<bool()>& cancelled);
ImagePixels orient_image(ImagePixels image, const AVFrame& frame);
}  // namespace lumaframevr
