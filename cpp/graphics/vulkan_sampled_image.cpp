#include "vulkan_sampled_image.hpp"
#include <stdexcept>
namespace lumaframevr {
VulkanSampledImage::VulkanSampledImage(std::shared_ptr<VulkanDevice> device, std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgba, std::shared_ptr<VulkanSampling> reuse)
    : device_(std::move(device)), sampling_(std::move(reuse)) {
    if(!width || !height || rgba.size() != static_cast<std::size_t>(width) * height * 4)
        throw std::invalid_argument("Invalid RGBA texture");
    try {
        create_image(VK_FORMAT_R8G8B8A8_UNORM, width, height, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        if(sampling_ && !sampling_->matches(device_, nullptr))
            sampling_.reset();
        create_sampling(VK_FORMAT_R8G8B8A8_UNORM);
        upload_rgba(width, height, rgba);
    } catch(...) {
        reset();
        throw;
    }
}
void VulkanSampledImage::create_image(VkFormat format, std::uint32_t width, std::uint32_t height, VkImageUsageFlags usage) {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.extent = {width, height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vulkan_check(vkCreateImage(device_->handle(), &info, nullptr, &image_), "create sampled image");
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device_->handle(), image_, &requirements);
    VkMemoryAllocateInfo memory{};
    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = device_->memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vulkan_check(vkAllocateMemory(device_->handle(), &memory, nullptr, &memory_), "allocate sampled image");
    vulkan_check(vkBindImageMemory(device_->handle(), image_, memory_, 0), "bind sampled image");
}
void VulkanSampledImage::create_sampling(VkFormat format) {
    if(!sampling_)
        sampling_ = std::make_shared<VulkanSampling>(device_);
    VkSamplerYcbcrConversionInfo conversion{};
    conversion.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO;
    conversion.conversion = sampling_->conversion();
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = conversion.conversion ? &conversion : nullptr;
    view.image = image_;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = format;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vulkan_check(vkCreateImageView(device_->handle(), &view, nullptr, &view_), "create sampled image view");
}
void VulkanSampledImage::reset() noexcept {
    if(view_)
        vkDestroyImageView(device_->handle(), view_, nullptr);
    if(image_)
        vkDestroyImage(device_->handle(), image_, nullptr);
    if(memory_)
        vkFreeMemory(device_->handle(), memory_, nullptr);
    sampling_.reset();
    view_ = {};
    image_ = {};
    memory_ = {};
}
VulkanSampledImage::~VulkanSampledImage() {
    reset();
}
}  // namespace lumaframevr
