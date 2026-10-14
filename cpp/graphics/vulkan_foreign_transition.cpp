#include "vulkan_foreign_transition.hpp"

namespace lumaframevr {
ForeignImageTransition foreign_image_transition(VkImage image, std::uint32_t family, bool acquire) {
    constexpr VkPipelineStageFlags shaders = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    ForeignImageTransition result;
    auto& barrier = result.barrier;
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcQueueFamilyIndex = acquire ? VK_QUEUE_FAMILY_FOREIGN_EXT : family;
    barrier.dstQueueFamilyIndex = acquire ? family : VK_QUEUE_FAMILY_FOREIGN_EXT;
    barrier.oldLayout = acquire ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = acquire ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = acquire ? 0 : VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = acquire ? VK_ACCESS_SHADER_READ_BIT : 0;
    result.source = acquire ? static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) : shaders;
    result.destination = acquire ? shaders : static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    return result;
}
void record_foreign_transition(VkCommandBuffer command, const ForeignImageTransition& transition) {
    vkCmdPipelineBarrier(command, transition.source, transition.destination, 0, 0, nullptr, 0, nullptr, 1, &transition.barrier);
}
}  // namespace lumaframevr
