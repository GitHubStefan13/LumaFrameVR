#pragma once
#include "vulkan_device.hpp"
#include "vulkan_host_buffer.hpp"
#include "vulkan_depth_attachment.hpp"
namespace lumaframevr {
class VulkanRenderTarget {
   public:
    VulkanRenderTarget(std::shared_ptr<VulkanDevice> device, std::uint32_t width, std::uint32_t height, VkFormat format, bool depth = false);
    VulkanRenderTarget(std::shared_ptr<VulkanDevice> device, VkImage image, VkExtent2D extent, VkFormat format, std::uint32_t eye, bool depth = false);
    ~VulkanRenderTarget();
    VulkanRenderTarget(const VulkanRenderTarget&) = delete;
    VulkanRenderTarget& operator=(const VulkanRenderTarget&) = delete;
    VkRenderPass render_pass() const { return pass_; }
    VkExtent2D extent() const { return extent_; }
    void begin(VkCommandBuffer command, float clear_alpha = 1) const;
    void end(VkCommandBuffer command) const;
    void read(VkCommandBuffer command, const VulkanHostBuffer& output) const;

   private:
    void initialize(VkFormat format, std::uint32_t eye);
    void reset() noexcept;
    std::shared_ptr<VulkanDevice> device_;
    VkImage image_{};
    VkDeviceMemory memory_{};
    VkImageView view_{};
    VkRenderPass pass_{};
    VkFramebuffer framebuffer_{};
    VkExtent2D extent_{};
    bool owns_image_{};
    std::unique_ptr<VulkanDepthAttachment> depth_;
};
}  // namespace lumaframevr
