#pragma once
#include "vulkan_device.hpp"
#include <span>
namespace lumaframevr {
struct UploadTransition {
    VkImageMemoryBarrier barrier;
    VkPipelineStageFlags source, destination;
};
UploadTransition upload_transition(VkImage image, bool sampled);
void upload_sampled_image(std::shared_ptr<VulkanDevice> device, VkBuffer staging, VkImage image, std::span<const VkBufferImageCopy> copies);
}  // namespace lumaframevr
