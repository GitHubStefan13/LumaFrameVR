#include "v4l2_format.hpp"
#include <linux/videodev2.h>

std::optional<media::V4l2PlaneLayout> media::v4l2_plane_layout(uint32_t format, uint32_t width, uint32_t height, uint64_t pitch) {
    if(!width || !height || !pitch)
        return {};
    if(format == v4l2_q10c) {
        // The driver's Q10C -> DRM P010 mapping. These are compressed storage strides, not two-byte
        // linear samples.
        const auto align_up = [](uint64_t value, uint64_t alignment) -> std::optional<uint64_t> {
            const uint64_t padding = (alignment - value % alignment) % alignment;
            if(value > UINT64_MAX - padding)
                return {};
            return value + padding;
        };
        const uint64_t meta_stride = *align_up((uint64_t(width) + 47) / 48, 64);
        const uint64_t meta_height = *align_up((uint64_t(height) + 3) / 4, 16);
        if(pitch > UINT64_MAX / height)
            return {};
        const auto metadata = align_up(meta_stride * meta_height, 4096);
        const auto luma = align_up(pitch * height, 4096);
        if(!metadata || !luma || *luma >= UINT64_MAX - *metadata)
            return {};
        const uint64_t uv = *metadata + *luma;
        // Compression has no linear row-sized payload bound: both plane starts must be
        // inside the driver's allocation. Vulkan independently checks its real import
        // memory requirements later.
        return V4l2PlaneLayout{v4l2_p010, width, height, {pitch, pitch}, {0, uv}, {}, uv + 1, v4l2_qcom_compressed};
    }
    if(format != V4L2_PIX_FMT_NV12 && format != v4l2_p010)
        return {};
    const uint64_t sample_bytes = format == v4l2_p010 ? 2 : 1;
    // Odd widths need a complete interleaved UV pair; odd heights need the last
    // chroma row. Use the driver's allocated extent, never the visible crop.
    const uint64_t uv_width = (uint64_t(width) + 1) / 2 * 2;
    const uint64_t chroma_rows = (uint64_t(height) + 1) / 2;
    const uint64_t rows = uint64_t(height) + chroma_rows;
    if(pitch < uv_width * sample_bytes || pitch % sample_bytes || pitch > UINT64_MAX / rows)
        return {};
    return V4l2PlaneLayout{format, width, height, {pitch, pitch}, {0, pitch * height}, {height, chroma_rows}, pitch * rows};
}
bool media::v4l2_buffer_fits(const V4l2PlaneLayout& layout, uint64_t size) {
    if(!layout.width || !layout.height || !layout.required_bytes || size < layout.required_bytes)
        return false;
    if(layout.modifier) {
        return layout.modifier == v4l2_qcom_compressed && layout.format == v4l2_p010 && layout.pitches[0] && layout.pitches[1] && layout.offsets[0] < size && layout.offsets[1] < size;
    }
    for(size_t i = 0; i < 2; ++i) {
        if(!layout.pitches[i] || !layout.rows[i] || layout.offsets[i] > size || layout.rows[i] > (size - layout.offsets[i]) / layout.pitches[i])
            return false;
    }
    return true;
}

std::optional<media::V4l2Formats> media::v4l2_formats(V4l2Stream stream) {
    if(!stream.width || !stream.height || !stream.chroma_420 || (stream.bits != 8 && stream.bits != 10))
        return {};
    uint32_t coded;
    switch(stream.codec) {
        case V4l2Codec::H264:
            coded = V4L2_PIX_FMT_H264;
            break;
        case V4l2Codec::Hevc:
            coded = V4L2_PIX_FMT_HEVC;
            break;
        case V4l2Codec::Vp9:
            coded = V4L2_PIX_FMT_VP9;
            break;
        default:
            return {};
    }
    return V4l2Formats{coded, stream.bits == 10 ? v4l2_q10c : V4L2_PIX_FMT_NV12};
}
