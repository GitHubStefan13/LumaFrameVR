#pragma once
#include "vulkan_device.hpp"
namespace lumaframevr {
class VulkanSubmission {
   public:
    explicit VulkanSubmission(std::shared_ptr<VulkanDevice> device);
    ~VulkanSubmission();
    VulkanSubmission(const VulkanSubmission&) = delete;
    VulkanSubmission& operator=(const VulkanSubmission&) = delete;
    VkCommandBuffer command() const { return command_; }
    void submit();
    void wait();

   private:
    void reset() noexcept;
    std::shared_ptr<VulkanDevice> device_;
    VkCommandPool pool_{};
    VkCommandBuffer command_{};
    VkFence fence_{};
    bool submitted_{};
};
}  // namespace lumaframevr
