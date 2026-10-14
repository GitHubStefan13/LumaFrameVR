#include "software_pixel_payload.hpp"
#include <algorithm>
#include <stdexcept>
#include "media_types.hpp"
extern "C" {
#include <libavutil/pixfmt.h>
}

namespace lumaframevr {
SoftwarePixelLayout software_layout(int ffmpeg_format, int planes) {
    switch(ffmpeg_format) {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVJ420P:
            if(planes == 3)
                return {8, 1, 0, true};
            break;
        case AV_PIX_FMT_NV12:
            if(planes == 2)
                return {8, 1, 0, false};
            break;
        case AV_PIX_FMT_YUV420P10LE:
            if(planes == 3)
                return {10, 2, 6, true};
            break;
        case AV_PIX_FMT_P010LE:
            if(planes == 2)
                return {10, 2, 0, false};
            break;
        default:
            break;
    }
    throw std::invalid_argument("Unsupported software 4:2:0 format or plane count");
}
namespace {
void shift_samples(std::vector<unsigned char>& data, unsigned shift) {
    if(!shift)
        return;
    for(std::size_t i = 0; i + 1 < data.size(); i += 2) {
        const auto value = static_cast<unsigned>(data[i]) | (static_cast<unsigned>(data[i + 1]) << 8);
        const auto shifted = value << shift;
        data[i] = static_cast<unsigned char>(shifted & 255);
        data[i + 1] = static_cast<unsigned char>((shifted >> 8) & 255);
    }
}
}  // namespace
SoftwarePixelPayload pack_software_420(const RetainedFrame& frame) {
    if(!frame.software())
        throw std::logic_error("DMA-BUF must be imported, not copied as CPU pixels");
    const auto& native = *frame.native_frame();
    const auto layout = software_layout(frame.format(), native.planes);
    if(frame.width() <= 0 || frame.height() <= 0)
        throw std::invalid_argument("Empty decoded picture");
    const auto width = (static_cast<std::uint32_t>(frame.width()) + 1U) & ~1U;
    const auto height = (static_cast<std::uint32_t>(frame.height()) + 1U) & ~1U;
    const auto row_bytes = static_cast<std::size_t>(width) * layout.sample_bytes;
    SoftwarePixelPayload result{width, height, layout.bits, layout.sample_shift, std::vector<unsigned char>(row_bytes * height), std::vector<unsigned char>(row_bytes * (height / 2))};
    for(int row = 0; row < frame.height(); ++row) {
        const auto source = frame.plane_row(0, row);
        std::copy(source.begin(), source.end(), result.luma.begin() + static_cast<std::size_t>(row) * row_bytes);
    }
    const auto chroma_rows = (frame.height() + 1) / 2;
    for(int row = 0; row < chroma_rows; ++row) {
        auto destination = result.chroma.begin() + static_cast<std::size_t>(row) * row_bytes;
        const auto u = frame.plane_row(1, row);
        if(!layout.planar)
            std::copy(u.begin(), u.end(), destination);
        else {
            const auto v = frame.plane_row(2, row);
            const auto samples = u.size() / layout.sample_bytes;
            for(std::size_t column = 0; column < samples; ++column) {
                for(unsigned byte = 0; byte < layout.sample_bytes; ++byte) {
                    destination[(column * 2) * layout.sample_bytes + byte] = u[column * layout.sample_bytes + byte];
                    destination[(column * 2 + 1) * layout.sample_bytes + byte] = v[column * layout.sample_bytes + byte];
                }
            }
        }
    }
    shift_samples(result.luma, layout.sample_shift);
    shift_samples(result.chroma, layout.sample_shift);
    return result;
}
}  // namespace lumaframevr
