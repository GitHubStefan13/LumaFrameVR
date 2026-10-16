#pragma once
#include "media_types.hpp"
#include <vulkan/vulkan.h>
#include <array>

namespace lumaframevr {
inline std::array<VkSubresourceLayout, 2> dma_buf_plane_layouts(const media::DmaBuf& source) {
    std::array<VkSubresourceLayout, 2> planes{};
    for(size_t i = 0; i < planes.size(); ++i) {
        planes[i].offset = source.offsets[i];
        planes[i].rowPitch = source.pitches[i];
    }
    return planes;
}

enum class DmaBufSizeAction { Import, DeriveLayout, Reject };
// Where the described planes actually end: the last byte any plane's rows touch, which is the memory
// the GPU can be asked to read. A two-plane 4:2:0 capture has a full-height luma plane and a
// half-height chroma plane, and both carry the driver's own pitches and offsets.
inline VkDeviceSize dma_buf_described_extent(const media::DmaBuf& source) {
    const VkDeviceSize rows[2] = {static_cast<VkDeviceSize>(source.height > 0 ? source.height : 0), static_cast<VkDeviceSize>(source.height > 0 ? (source.height + 1) / 2 : 0)};
    // The crop is inside the coded extent on this path; a zero pitch would describe nothing.
    VkDeviceSize extent = 0;
    for(std::size_t i = 0; i < 2; ++i) {
        if(source.pitches[i] == 0)
            continue;
        const VkDeviceSize end = source.offsets[i] + rows[i] * source.pitches[i];
        if(end > extent)
            extent = end;
    }
    return extent;
}
inline DmaBufSizeAction dma_buf_size_action(const media::DmaBuf& source, VkDeviceSize required, bool retried = false) {
    if(required <= source.size)
        return DmaBufSizeAction::Import;
    // Vulkan's requirement for a modified two-plane capture is an estimate for a vendor layout it does
    // not model: the device run of a 7296x3648 Q10C capture was asked for 28672 bytes more than the
    // driver had allocated, while the planes it reported end 90112 bytes inside that allocation. What
    // has to fit in the buffer is the memory the described planes occupy, so where the producer
    // negotiated the layout that is the test - the requirement alone would refuse a frame whose
    // samples are all present. A layout whose planes do not fit is still refused.
    if(source.source_layout)
        return dma_buf_described_extent(source) <= source.size ? DmaBufSizeAction::Import : DmaBufSizeAction::Reject;
    // Recreating an identical explicit layout cannot reduce its requirements.
    // Only a computed layout may be discarded; a negotiated source layout fails closed.
    if(retried)
        return DmaBufSizeAction::Reject;
    return DmaBufSizeAction::DeriveLayout;
}

// Everything VkImageCreateInfo needs for a decoder DMA-BUF import, held in one
// object so the DRM modifier and the V4L2-reported plane layouts cannot drift
// apart. A modified import must ask for VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT
// and carry a VkImageDrmFormatModifierExplicitCreateInfoEXT with the source
// modifier and its plane offsets/pitches: created without them Vulkan sizes a
// linear image and reports a requirement the modified buffer cannot satisfy.
struct DmaBufImageCreateInfo {
    VkImageCreateInfo image{};
    VkExternalMemoryImageCreateInfo external{};
    VkImageDrmFormatModifierExplicitCreateInfoEXT explicit_layout{};
    std::array<VkSubresourceLayout, 2> planes{};
};

// Fills `out` and wires the pNext chain into `out`'s own storage, so `out` must
// be the object that reaches vkCreateImage and must never be copied: its
// internal pointers (image.pNext, external.pNext, pPlaneLayouts) describe it.
inline void build_dma_buf_image_create_info(const media::DmaBuf& source, VkFormat format, VkExternalMemoryHandleTypeFlagBits handle_type, DmaBufImageCreateInfo& out) {
    out.planes = dma_buf_plane_layouts(source);
    out.explicit_layout.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
    out.explicit_layout.drmFormatModifier = source.modifier;
    out.explicit_layout.drmFormatModifierPlaneCount = static_cast<std::uint32_t>(out.planes.size());
    out.explicit_layout.pPlaneLayouts = out.planes.data();
    out.external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    out.external.handleTypes = handle_type;
    out.external.pNext = &out.explicit_layout;
    out.image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    out.image.pNext = &out.external;
    out.image.imageType = VK_IMAGE_TYPE_2D;
    out.image.format = format;
    out.image.extent = {static_cast<std::uint32_t>(source.width), static_cast<std::uint32_t>(source.height), 1};
    out.image.mipLevels = 1;
    out.image.arrayLayers = 1;
    out.image.samples = VK_SAMPLE_COUNT_1_BIT;
    out.image.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    out.image.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
}
}  // namespace lumaframevr
