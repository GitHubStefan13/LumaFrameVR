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
inline DmaBufSizeAction dma_buf_size_action(const media::DmaBuf& source, VkDeviceSize required, bool retried = false) {
    if(required <= source.size)
        return DmaBufSizeAction::Import;
    // Recreating an identical explicit layout cannot reduce its requirements.
    // Only a computed layout may be discarded; a negotiated source layout fails closed.
    if(source.source_layout || retried)
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
