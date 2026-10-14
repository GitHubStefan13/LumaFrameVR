#include "vulkan_video_image.hpp"
#include "vulkan_host_buffer.hpp"
#include "vulkan_upload_transition.hpp"
#include <cstring>
namespace lumaframevr {
void VulkanVideoImage::upload_yuv(const SoftwarePixelPayload& pixels) {
    std::size_t chroma_offset = (pixels.luma.size() + 3) & ~std::size_t(3);
    VulkanHostBuffer staging(device_, chroma_offset + pixels.chroma.size());
    std::memcpy(staging.bytes().data(), pixels.luma.data(), pixels.luma.size());
    std::memcpy(staging.bytes().data() + chroma_offset, pixels.chroma.data(), pixels.chroma.size());
    std::array<VkBufferImageCopy, 2> copies{};
    copies[0].imageSubresource = {VK_IMAGE_ASPECT_PLANE_0_BIT, 0, 0, 1};
    copies[0].imageExtent = {static_cast<std::uint32_t>(pixels.width), static_cast<std::uint32_t>(pixels.height), 1};
    copies[1].bufferOffset = chroma_offset;
    copies[1].imageSubresource = {VK_IMAGE_ASPECT_PLANE_1_BIT, 0, 0, 1};
    copies[1].imageExtent = {static_cast<std::uint32_t>((pixels.width + 1) / 2), static_cast<std::uint32_t>((pixels.height + 1) / 2), 1};
    upload_sampled_image(device_, staging.handle(), image_, copies);
}
}  // namespace lumaframevr
