#include "test_registry.hpp"
#include "test_assertions.hpp"
#include "v4l2_format.hpp"
#include <linux/videodev2.h>
#include <limits>

namespace {
// The one device recording of the Q10C size disagreement, at 7296x3648 with the driver's pitch. Named
// because the arithmetic below reads in terms of them, and because a reader has to be able to see
// that they are evidence rather than values this code chose.
constexpr uint64_t recorded_width = 7296;
constexpr uint64_t recorded_height = 3648;
constexpr uint64_t recorded_pitch = 9728;
constexpr uint64_t recorded_plane_offsets[] = {0, 35663872};
constexpr uint64_t recorded_driver_buffer = 53497856;
constexpr uint64_t recorded_vulkan_requirement = 53526528;
// chroma offset + half the luma rows, which is where the two planes the driver reported end.
constexpr uint64_t recorded_planes_end = recorded_plane_offsets[1] + (recorded_height + 1) / 2 * recorded_pitch;
static_assert(recorded_planes_end < recorded_driver_buffer, "the recorded planes end inside the recorded allocation");
static_assert(recorded_driver_buffer < recorded_vulkan_requirement, "the recorded requirement is the larger number");
}  // namespace

int lumaframevr::tests::v4l2_layout(int, char**) {
    return acceptance_main([] {
        struct Case {
            uint32_t format, width, height;
            uint64_t pitch, chroma_rows, offset, bytes;
        };
        const Case cases[] = {
                {V4L2_PIX_FMT_NV12, 96, 64, 96, 32, 6144, 9216},
                {V4L2_PIX_FMT_NV12, 95, 65, 128, 33, 8320, 12544},
                {media::v4l2_p010, 96, 64, 192, 32, 12288, 18432},
                {media::v4l2_p010, 95, 65, 256, 33, 16640, 25088},
                {V4L2_PIX_FMT_NV12, 7296, 3648, 7296, 1824, 26615808, 39923712},
                {media::v4l2_p010, 7296, 3648, 14592, 1824, 53231616, 79847424},
                // The allocated height includes driver padding; cropping does not shrink UV.
                {media::v4l2_p010, 7296, 3680, 14848, 1840, 54640640, 81960960},
                {media::v4l2_p010, 1, 1, 4, 1, 4, 8},
        };
        for(const auto& item : cases) {
            auto layout = media::v4l2_plane_layout(item.format, item.width, item.height, item.pitch);
            require(layout.has_value(), "supported linear layout");
            require(layout->format == item.format && layout->width == item.width && layout->height == item.height, "layout preserves allocated geometry");
            require(layout->pitches == std::array<uint64_t, 2>{item.pitch, item.pitch}, "both pitches preserved");
            require(layout->rows == std::array<uint64_t, 2>{item.height, item.chroma_rows}, "ceil chroma rows");
            require(layout->offsets == std::array<uint64_t, 2>{0, item.offset}, "chroma after full luma plane");
            require(layout->required_bytes == item.bytes && layout->modifier == 0, "linear extent and no DRM modifier unchanged");
            require(media::v4l2_buffer_fits(*layout, item.bytes), "exact size accepted");
            require(media::v4l2_buffer_fits(*layout, item.bytes + 4096), "larger allocation accepted without moving chroma");
            require(!media::v4l2_buffer_fits(*layout, item.bytes - 1), "one-byte shortage rejected");
            if(item.bytes > 28672)
                require(!media::v4l2_buffer_fits(*layout, item.bytes - 28672), "28672-byte chroma shortage rejected");
        }
        // Historical Vulkan import requirement, not a claim that the vendor's
        // compressed Q10C allocation is linear P010. Reproduce the exact size gate.
        auto historical = *media::v4l2_plane_layout(V4L2_PIX_FMT_NV12, 96, 64, 96);
        historical.required_bytes = 53526528;
        require(!media::v4l2_buffer_fits(historical, 53497856), "historical offered buffer is 28672 bytes short");
        require(media::v4l2_buffer_fits(historical, 53526528), "historical required size accepted");
        for(uint32_t format : {V4L2_PIX_FMT_NV12, media::v4l2_p010}) {
            require(!media::v4l2_plane_layout(format, 0, 64, 256), "zero width rejected");
            require(!media::v4l2_plane_layout(format, 96, 0, 256), "zero height rejected");
            require(!media::v4l2_plane_layout(format, 96, 64, 0), "zero pitch rejected");
            require(!media::v4l2_plane_layout(format, 95, 65, format == media::v4l2_p010 ? 190 : 95), "pitch must hold rounded-up UV pairs");
            require(!media::v4l2_plane_layout(format, 96, 64, std::numeric_limits<uint64_t>::max()), "multiplication overflow rejected");
            require(!media::v4l2_plane_layout(format, 1, 1, std::numeric_limits<uint64_t>::max() / 2 + 1), "sum overflow rejected");
        }
        require(!media::v4l2_plane_layout(media::v4l2_p010, 96, 64, 193), "P010 pitch sample-aligned");
        const auto q10c = v4l2_fourcc('Q', '1', '0', 'C');
        struct Compressed {
            uint32_t width, height;
            uint64_t pitch, uv;
        };
        for(const auto& item : {Compressed{7296, 3648, 9728, 35663872}, Compressed{1937, 1081, 2624, 2859008}}) {
            auto layout = media::v4l2_plane_layout(q10c, item.width, item.height, item.pitch);
            require(layout.has_value(), "documented Q10C mapping is exportable");
            require(layout->format == media::v4l2_p010 && layout->pitches == std::array<uint64_t, 2>{item.pitch, item.pitch} && layout->offsets == std::array<uint64_t, 2>{0, item.uv},
                    "Q10C exports P010 with driver pitch and page-aligned metadata/luma UV offset");
            require(media::v4l2_buffer_fits(*layout, item.uv + 1), "compressed extent is not a linear P010 row-size requirement");
            require(!media::v4l2_buffer_fits(*layout, item.uv), "compressed UV plane must start inside allocation");
            // What the queue asks the driver for, for this compressed capture: the bytes the planes
            // occupy plus the media layer's own margin, so that the allocation can cover the
            // requirement Vulkan reports for the same modified format instead of relying on the
            // planes alone. The margin is not restated here; it is the constant the request uses.
            const uint64_t described = item.uv + item.pitch * ((item.height + 1) / 2);
            require(media::v4l2_requested_capture_size(*layout) == described + media::v4l2_capture_margin_bytes(described),
                    "the capture size request is the described planes plus the margin for that extent");
        }
        {
            // The recorded geometry, against the numbers the device reported: the driver allocated
            // 53497856, Vulkan asked for 53526528, and the planes end at 53407744. The request needs
            // to reach past Vulkan's number, or there is no point asking for it at all.
            const auto recorded = *media::v4l2_plane_layout(q10c, recorded_width, recorded_height, recorded_pitch);
            require(recorded.offsets[1] == recorded_plane_offsets[1], "the recorded chroma offset is what the mapping derives");
            require(media::v4l2_requested_capture_size(recorded) >= recorded_vulkan_requirement, "the request covers the requirement the device reported");
            require(media::v4l2_requested_capture_size(recorded) <= recorded_driver_buffer + recorded_vulkan_requirement - recorded_planes_end,
                    "the request is a margin on the planes, not a second allocation");
            // The margin is proportional above the floor and the floor below it, which is what makes it
            // grow with the frame instead of being a fixed number of pages at every resolution.
            require(media::v4l2_capture_margin_bytes(media::v4l2_capture_margin_floor_bytes * media::v4l2_capture_margin_divisor) == media::v4l2_capture_margin_floor_bytes,
                    "an extent at the floor's own ratio is still asked for the floor");
            require(media::v4l2_capture_margin_bytes(recorded_planes_end) == recorded_planes_end / media::v4l2_capture_margin_divisor,
                    "the recorded extent is large enough that the proportional part is the margin");
        }
        require(media::v4l2_requested_capture_size(*media::v4l2_plane_layout(V4L2_PIX_FMT_NV12, 96, 64, 96)) == 0, "a linear capture is not asked for a margin: the driver's size is the layout");
        require(!media::v4l2_plane_layout(q10c, 0, 64, 96), "Q10C zero width rejected");
        require(!media::v4l2_plane_layout(q10c, 96, 0, 96), "Q10C zero height rejected");
        require(!media::v4l2_plane_layout(q10c, 96, 64, 0), "Q10C zero pitch rejected");
        require(!media::v4l2_plane_layout(q10c, 96, 64, UINT64_MAX), "Q10C multiplication overflow rejected");
        require(!media::v4l2_plane_layout(q10c, 1, 1, UINT64_MAX - 4095), "Q10C aligned sum overflow rejected");
        require(!media::v4l2_plane_layout(v4l2_fourcc('Q', '0', '8', 'C'), 96, 64, 96), "undocumented vendor mapping stays fail-closed");
    });
}

int lumaframevr::tests::v4l2_formats(int, char**) {
    return acceptance_main([] {
        struct Case {
            media::V4l2Codec codec;
            uint32_t coded;
        };
        for(const auto& item : {Case{media::V4l2Codec::H264, V4L2_PIX_FMT_H264}, Case{media::V4l2Codec::Hevc, V4L2_PIX_FMT_HEVC}, Case{media::V4l2Codec::Vp9, V4L2_PIX_FMT_VP9}}) {
            for(uint32_t bits : {8u, 10u}) {
                auto formats = media::v4l2_formats({item.codec, 7296, 3648, bits});
                require(formats.has_value(), "supported stream selects formats");
                require(formats->coded == item.coded, "coded fourcc follows codec");
                require(formats->capture == (bits == 8 ? V4L2_PIX_FMT_NV12 : v4l2_fourcc('Q', '1', '0', 'C')), "capture requests Q10C directly for ten-bit, NV12 otherwise");
            }
        }
        require(!media::v4l2_formats({media::V4l2Codec::Unsupported, 96, 64, 8}), "unknown codec rejected");
        require(!media::v4l2_formats({media::V4l2Codec::Hevc, 0, 64, 8}), "empty geometry rejected");
        require(!media::v4l2_formats({media::V4l2Codec::Hevc, 96, 64, 12}), "unsupported depth rejected");
        require(!media::v4l2_formats({media::V4l2Codec::Hevc, 96, 64, 10, false}), "non-420 rejected");
    });
}
