#include "vulkan_sampled_image.hpp"
#include "vulkan_host_buffer.hpp"
#include "vulkan_upload_transition.hpp"
#include <cstring>
namespace lumaframevr {
void VulkanSampledImage::upload_rgba(std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgba) {
    VulkanHostBuffer staging(device_, rgba.size());
    std::memcpy(staging.bytes().data(), rgba.data(), rgba.size());
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {width, height, 1};
    upload_sampled_image(device_, staging.handle(), image_, std::span(&copy, 1));
}
}  // namespace lumaframevr
