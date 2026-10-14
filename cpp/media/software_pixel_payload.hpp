#pragma once
#include "decoder_session.hpp"
#include <cstdint>
#include <vector>

namespace lumaframevr {
struct SoftwarePixelLayout {
    unsigned bits, sample_bytes, sample_shift;
    bool planar;
};
struct SoftwarePixelPayload {
    std::uint32_t width, height;
    unsigned bits, sample_shift;
    std::vector<unsigned char> luma, chroma;
};
SoftwarePixelLayout software_layout(int ffmpeg_format, int planes);
SoftwarePixelPayload pack_software_420(const RetainedFrame& frame);
}  // namespace lumaframevr
