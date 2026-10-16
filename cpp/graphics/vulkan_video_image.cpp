#include "vulkan_video_image.hpp"
#include <stdexcept>
#include <iostream>
#include "vulkan_submission.hpp"
#include "vulkan_foreign_transition.hpp"
namespace lumaframevr {
VkFormat dma_buf_format(std::uint32_t format, int bits) {
    if(format == 0x3231564e && bits <= 8)
        return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
    if(format == 0x30313050)
        return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
    return VK_FORMAT_UNDEFINED;
}
VkFormat software_yuv_format(unsigned bits) {
    return bits > 8 ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 : VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
}
std::array<float, 4> dma_buf_crop(int width, int height, std::array<int, 4> crop) {
    if(width <= 0 || height <= 0 || crop[0] < 0 || crop[1] < 0 || crop[2] < 0 || crop[3] < 0 || crop[0] + crop[2] >= width || crop[1] + crop[3] >= height)
        throw std::invalid_argument("Invalid coded DMA-BUF extent or crop");
    return {static_cast<float>(crop[0]) / width, static_cast<float>(crop[1]) / height, static_cast<float>(width - crop[0] - crop[2]) / width, static_cast<float>(height - crop[1] - crop[3]) / height};
}
VulkanVideoImage::VulkanVideoImage(std::shared_ptr<VulkanDevice> device, RetainedFrame frame, std::shared_ptr<VulkanSampling> reuse)
    : VulkanSampledImage(std::move(device), std::move(reuse)), frame_(std::move(frame)) {
    if(!frame_.valid())
        throw std::invalid_argument("Video texture requires retained decoder frame");
    try {
        if(!frame_.software()) {
            import_dma();
            return;
        }
        auto pixels = pack_software_420(frame_);
        VkFormat format = software_yuv_format(pixels.bits);
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(device_->physical(), format, &properties);
        if(!(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
            throw std::runtime_error("GPU does not support software YUV sampling format");
        VkPhysicalDeviceImageFormatInfo2 info{};
        info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
        info.format = format;
        info.type = VK_IMAGE_TYPE_2D;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkSamplerYcbcrConversionImageFormatProperties conversion{};
        conversion.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES;
        VkImageFormatProperties2 image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
        image.pNext = &conversion;
        vulkan_check(vkGetPhysicalDeviceImageFormatProperties2(device_->physical(), &info, &image), "query YUV image format");
        descriptor_count_ = conversion.combinedImageSamplerDescriptorCount;
        create_image(format, static_cast<std::uint32_t>(pixels.width), static_cast<std::uint32_t>(pixels.height), info.usage);
        create_conversion(format, properties.optimalTilingFeatures);
        create_sampling(format);
        upload_yuv(pixels);
    } catch(...) {
        reset();
        throw;
    }
}
void VulkanVideoImage::release_foreign() {
    if(!foreign_owned_)
        return;
    VulkanSubmission release(device_);
    record_foreign_transition(release.command(), foreign_image_transition(image_, device_->queue_family(), false));
    release.submit();
    release.wait();
    foreign_owned_ = false;
}
VulkanVideoImage::~VulkanVideoImage() {
    try {
        release_foreign();
    } catch(const std::exception& error) {
        std::cerr << "DMA-BUF release failed: " << error.what() << '\n';
    }
    reset();
}
}  // namespace lumaframevr
