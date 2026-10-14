#include "test_registry.hpp"
#include "test_assertions.hpp"
#include "vulkan_dma_layout.hpp"
#include "vulkan_video_image.hpp"
#include "v4l2_format.hpp"
#include <array>
#include <cstdint>

// The DRM modifier a Q10C (compressed ten-bit) capture reports must reach
// vkCreateImage together with the driver's plane offsets and pitches. If the
// image is created without VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT and a
// VkImageDrmFormatModifierExplicitCreateInfoEXT carrying the source modifier,
// Vulkan sizes a linear image and asks for more than the modified buffer holds.
// Values below are the ones the device reported for a 7296x3648 Q10C capture.
namespace {
constexpr uint64_t q10c_modifier = 0x500000000000001;  // media::v4l2_qcom_compressed
constexpr uint64_t chroma_offset = 35663872;           // metadata (0x2B000) + page-aligned luma
constexpr uint64_t stored_pitch = 9728;                // driver stride, not 2-byte linear samples
constexpr uint64_t driver_buffer = 53497856;           // V4L2 negotiated allocation
constexpr uint64_t vulkan_requirement = 53526528;      // 28672 bytes larger, the reported shortfall
}  // namespace

int lumaframevr::tests::dma_import(int, char**) {
    return acceptance_main([] {
        // 1. The reported source layout: what the Q10C formula predicts.
        auto reported = media::v4l2_plane_layout(media::v4l2_q10c, 7296, 3648, stored_pitch);
        require(reported.has_value(), "Q10C capture maps to a DRM P010 layout");
        require(reported->format == media::v4l2_p010, "Q10C exports P010");
        require(reported->modifier == q10c_modifier, "Q10C carries the vendor DRM modifier");
        require(reported->offsets == std::array<uint64_t, 2>{0, chroma_offset}, "chroma offset is the metadata+luma boundary");
        require(reported->pitches == std::array<uint64_t, 2>{stored_pitch, stored_pitch}, "both pitches are the driver stride");

        // 2. What the media layer hands the graphics layer for that frame.
        media::DmaBuf source{};
        source.fd = 0;
        source.format = reported->format;
        source.width = 7296;
        source.height = 3648;
        source.size = driver_buffer;
        source.modifier = reported->modifier;
        source.offsets[0] = reported->offsets[0];
        source.offsets[1] = reported->offsets[1];
        source.pitches[0] = reported->pitches[0];
        source.pitches[1] = reported->pitches[1];
        source.source_layout = true;  // a negotiated V4L2 layout, authoritative

        const auto layouts = dma_buf_plane_layouts(source);
        require(layouts[0].offset == 0 && layouts[0].rowPitch == stored_pitch, "reported luma offset and pitch become the Vulkan subresource layout");
        require(layouts[1].offset == chroma_offset && layouts[1].rowPitch == stored_pitch, "reported chroma offset and pitch become the Vulkan subresource layout");
        require(layouts[0].depthPitch == 0 && layouts[1].arrayPitch == 0, "single-image planes carry no array or depth pitch");

        // 3. The creation info that goes to vkCreateImage.
        const VkFormat format = dma_buf_format(source.format, 10);
        require(format == VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, "P010 selects the two-plane 10-bit format");
        DmaBufImageCreateInfo create_info;
        build_dma_buf_image_create_info(source, format, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, create_info);

        require(create_info.image.tiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT, "import image is created as a modified image, not a linear one");
        require(create_info.image.format == format, "import image keeps the imported format");
        require(create_info.image.extent.width == 7296 && create_info.image.extent.height == 3648 && create_info.image.extent.depth == 1, "import image keeps the coded extent");
        require(create_info.image.usage == VK_IMAGE_USAGE_SAMPLED_BIT, "import image is sampleable");

        // The modifier and the reported planes are reachable from the exact
        // VkImageCreateInfo handed to vkCreateImage.
        require(create_info.image.pNext == &create_info.external, "external-memory info is in the image's pNext chain");
        require(create_info.external.handleTypes == VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, "DMA-BUF handle type carried");
        require(create_info.external.pNext == &create_info.explicit_layout, "explicit-modifier info follows the external-memory info");
        require(create_info.explicit_layout.drmFormatModifier == q10c_modifier, "source DRM modifier reaches image creation");
        require(create_info.explicit_layout.drmFormatModifierPlaneCount == 2, "both planes are described");
        require(create_info.explicit_layout.pPlaneLayouts == create_info.planes.data(), "Vulkan gets the reported plane layouts");
        require(create_info.planes[0].offset == 0 && create_info.planes[0].rowPitch == stored_pitch, "luma plane offset 0 and pitch 9728 preserved");
        require(create_info.planes[1].offset == chroma_offset && create_info.planes[1].rowPitch == stored_pitch, "chroma plane offset 35663872 and pitch 9728 preserved");

        // 4. The 28672-byte disagreement is reached with the modifier already in
        // place, so it is not a missing-modifier fault: the explicit layout is
        // what makes Vulkan ask for 53526528, and a negotiated layout fails
        // closed rather than binding a smaller allocation.
        require(dma_buf_size_action(source, vulkan_requirement, false) == DmaBufSizeAction::Reject, "negotiated layout never relaxes to a smaller allocation");
        media::DmaBuf computed = source;
        computed.source_layout = false;
        require(dma_buf_size_action(computed, vulkan_requirement, false) == DmaBufSizeAction::DeriveLayout, "computed layout may retry without an explicit layout");
        require(dma_buf_size_action(computed, vulkan_requirement, true) == DmaBufSizeAction::Reject, "computed layout gets only one retry");
        require(dma_buf_size_action(computed, driver_buffer) == DmaBufSizeAction::Import, "an allocation at the requirement imports");
    });
}
