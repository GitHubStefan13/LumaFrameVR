#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace media {
// P010 is absent from older Linux UAPI headers in the host SDK.
inline constexpr uint32_t v4l2_p010 = uint32_t('P') | (uint32_t('0') << 8) | (uint32_t('1') << 16) | (uint32_t('0') << 24);
inline constexpr uint32_t v4l2_q10c = uint32_t('Q') | (uint32_t('1') << 8) | (uint32_t('0') << 16) | (uint32_t('C') << 24);
inline constexpr uint64_t v4l2_qcom_compressed = UINT64_C(0x0500000000000001);
enum class V4l2Codec { H264, Hevc, Vp9, Unsupported };
struct V4l2Stream {
    V4l2Codec codec;
    uint32_t width, height, bits;
    bool chroma_420 = true;
};
struct V4l2Formats {
    uint32_t coded, capture;
};
struct V4l2PlaneLayout {
    uint32_t format, width, height;
    std::array<uint64_t, 2> pitches, offsets, rows;
    // Linear: full row extent. Q10C: minimum containing both plane starts,
    // with the actual compressed allocation size remaining driver-authoritative.
    uint64_t required_bytes;
    uint64_t modifier = 0;
};
// Linear NV12/P010 or the documented Q10C -> modified DRM P010 mapping.
// Height is the allocated (not cropped) driver height.
std::optional<V4l2Formats> v4l2_formats(V4l2Stream stream);
std::optional<V4l2PlaneLayout> v4l2_plane_layout(uint32_t format, uint32_t width, uint32_t height, uint64_t pitch);
bool v4l2_buffer_fits(const V4l2PlaneLayout& layout, uint64_t size);
}  // namespace media
