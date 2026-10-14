#include "vulkan_depth_attachment.hpp"
#include <stdexcept>
namespace lumaframevr {
VulkanDepthAttachment::VulkanDepthAttachment(std::shared_ptr<VulkanDevice> device, VkExtent2D extent) : device_(std::move(device)) {
    for(auto candidate : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM}) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(device_->physical(), candidate, &properties);
        if(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            format_ = candidate;
            break;
        }
    }
    if(!format_)
        throw std::runtime_error("No supported controller depth attachment format");
    try {
        VkImageCreateInfo create{};
        create.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        create.imageType = VK_IMAGE_TYPE_2D;
        create.format = format_;
        create.extent = {extent.width, extent.height, 1};
        create.mipLevels = 1;
        create.arrayLayers = 1;
        create.samples = VK_SAMPLE_COUNT_1_BIT;
        create.tiling = VK_IMAGE_TILING_OPTIMAL;
        create.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        vulkan_check(vkCreateImage(device_->handle(), &create, nullptr, &image_), "create controller depth image");
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_->handle(), image_, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = device_->memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vulkan_check(vkAllocateMemory(device_->handle(), &allocate, nullptr, &memory_), "allocate controller depth memory");
        vulkan_check(vkBindImageMemory(device_->handle(), image_, memory_, 0), "bind controller depth memory");
        VkImageViewCreateInfo view{};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = image_;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = format_;
        view.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        vulkan_check(vkCreateImageView(device_->handle(), &view, nullptr, &view_), "create controller depth view");
    } catch(...) {
        reset();
        throw;
    }
}
void VulkanDepthAttachment::reset() noexcept {
    if(view_)
        vkDestroyImageView(device_->handle(), view_, nullptr);
    if(image_)
        vkDestroyImage(device_->handle(), image_, nullptr);
    if(memory_)
        vkFreeMemory(device_->handle(), memory_, nullptr);
}
VulkanDepthAttachment::~VulkanDepthAttachment() {
    reset();
}
}  // namespace lumaframevr
