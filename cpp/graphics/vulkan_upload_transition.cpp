#include "vulkan_upload_transition.hpp"
#include "vulkan_submission.hpp"
namespace lumaframevr {
UploadTransition upload_transition(VkImage image, bool sampled) {
    UploadTransition result{};
    auto& barrier = result.barrier;
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = sampled ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = sampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = sampled ? VK_ACCESS_TRANSFER_WRITE_BIT : 0;
    barrier.dstAccessMask = sampled ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    result.source = sampled ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    result.destination = sampled ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT;
    return result;
}
void upload_sampled_image(std::shared_ptr<VulkanDevice> device, VkBuffer staging, VkImage image, std::span<const VkBufferImageCopy> copies) {
    VulkanSubmission upload(std::move(device));
    auto record = [&](bool sampled) {
        const auto step = upload_transition(image, sampled);
        vkCmdPipelineBarrier(upload.command(), step.source, step.destination, 0, 0, nullptr, 0, nullptr, 1, &step.barrier);
    };
    record(false);
    vkCmdCopyBufferToImage(upload.command(), staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(copies.size()), copies.data());
    record(true);
    upload.submit();
    upload.wait();
}
}  // namespace lumaframevr
