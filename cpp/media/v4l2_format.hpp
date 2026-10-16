#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace media {
// P010 is absent from older Linux UAPI headers in the host SDK.
inline constexpr uint32_t v4l2_p010 = uint32_t('P') | (uint32_t('0') << 8) | (uint32_t('1') << 16) | (uint32_t('0') << 24);
inline constexpr uint32_t v4l2_q10c = uint32_t('Q') | (uint32_t('1') << 8) | (uint32_t('0') << 16) | (uint32_t('C') << 24);
inline constexpr uint64_t v4l2_qcom_compressed = UINT64_C(0x0500000000000001);
// The granularity the Q10C mapping and the capture size request both round to. It is the driver's
// own: the metadata and luma regions in v4l2_plane_layout are aligned to it because the observed
// offsets are.
inline constexpr uint64_t v4l2_page_bytes = 4096;
// The margin the capture size request adds to the bytes the described planes occupy. Two parts,
// both out of the one device recording, where Vulkan asked 118784 bytes (29 pages) more than the
// planes occupy and the driver had allocated 90112 bytes (22 pages) more than them:
//
//   * a proportional part of described/256, which is 0.39%: the observed excess is 0.22% of the
//     described planes, so this leaves 1.8x the one measurement and grows with the frame, which a
//     fixed page count would not - the same shortfall at 8K is a larger number of bytes;
//   * a floor of 32 pages, for frames small enough that 0.39% of them is not worth asking for.
//
// What that costs, at the recorded 7296x3648: 208624 bytes a buffer, and the capture pool is eight
// buffers, so 1.6 MiB against a 428 MiB pool. It is headroom, not a correction - what makes a
// capture play is importing the negotiated layout on the planes the driver described, and the
// request only exists so the allocation can also satisfy Vulkan's requirement for the same modified
// format. V4L2 lets a client propose `sizeimage`; the readback decides, and a driver that keeps its
// own size is not asked again for that geometry.
inline constexpr uint64_t v4l2_capture_margin_floor_bytes = 32 * v4l2_page_bytes;
inline constexpr uint64_t v4l2_capture_margin_divisor = 256;
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
// The margin for a given described extent: the proportional part or the floor, whichever is larger.
uint64_t v4l2_capture_margin_bytes(uint64_t described);
// The capture size to ask the driver for, for a compressed Q10C capture: the bytes its described
// planes occupy plus v4l2_capture_margin_bytes. V4L2 lets a client propose sizeimage on S_FMT and the
// driver's readback decides, so a clamp costs nothing but the request.
uint64_t v4l2_requested_capture_size(const V4l2PlaneLayout& layout);
}  // namespace media
