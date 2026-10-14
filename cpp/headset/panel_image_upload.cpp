#include "panel_image_upload.hpp"
#include "vulkan_host_buffer.hpp"
#include "vulkan_submission.hpp"
#include <cstring>
namespace lumaframevr {
void upload_panel_image(std::shared_ptr<VulkanDevice> device, VkImage image, const ui::Canvas& canvas) {
    VulkanHostBuffer staging(device, canvas.pixels().size());
    const auto row_bytes = static_cast<std::size_t>(canvas.width()) * 4;
    for(int y = 0; y < canvas.height(); ++y) {
        const auto source = static_cast<std::size_t>(canvas.height() - 1 - y) * row_bytes;
        const auto destination = static_cast<std::size_t>(y) * row_bytes;
        std::memcpy(staging.bytes().data() + destination, canvas.pixels().data() + source, row_bytes);
    }
    VulkanSubmission transfer(device);
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(transfer.command(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {static_cast<std::uint32_t>(canvas.width()), static_cast<std::uint32_t>(canvas.height()), 1};
    vkCmdCopyBufferToImage(transfer.command(), staging.handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    vkCmdPipelineBarrier(transfer.command(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    transfer.submit();
    transfer.wait();
}
}  // namespace lumaframevr
