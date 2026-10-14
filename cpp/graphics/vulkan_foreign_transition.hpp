#pragma once
#include "vulkan_device.hpp"

namespace lumaframevr {
struct ForeignImageTransition {
    VkImageMemoryBarrier barrier{};
    VkPipelineStageFlags source{}, destination{};
};
ForeignImageTransition foreign_image_transition(VkImage image, std::uint32_t family, bool acquire);
void record_foreign_transition(VkCommandBuffer command, const ForeignImageTransition& transition);
}  // namespace lumaframevr
