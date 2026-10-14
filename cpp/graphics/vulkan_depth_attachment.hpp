#pragma once
#include "vulkan_device.hpp"
namespace lumaframevr {
class VulkanDepthAttachment {
   public:
    VulkanDepthAttachment(std::shared_ptr<VulkanDevice> device, VkExtent2D extent);
    ~VulkanDepthAttachment();
    VulkanDepthAttachment(const VulkanDepthAttachment&) = delete;
    VulkanDepthAttachment& operator=(const VulkanDepthAttachment&) = delete;
    VkImageView view() const { return view_; }
    VkFormat format() const { return format_; }

   private:
    void reset() noexcept;
    std::shared_ptr<VulkanDevice> device_;
    VkImage image_{};
    VkDeviceMemory memory_{};
    VkImageView view_{};
    VkFormat format_{};
};
}  // namespace lumaframevr
