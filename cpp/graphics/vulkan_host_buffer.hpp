#pragma once
#include "vulkan_device.hpp"
#include <span>
namespace lumaframevr {
class VulkanHostBuffer {
   public:
    VulkanHostBuffer(std::shared_ptr<VulkanDevice> device, VkDeviceSize size, VkBufferUsageFlags extra_usage = 0);
    ~VulkanHostBuffer();
    VulkanHostBuffer(const VulkanHostBuffer&) = delete;
    VulkanHostBuffer& operator=(const VulkanHostBuffer&) = delete;
    VkBuffer handle() const { return buffer_; }
    VkDeviceSize size() const { return size_; }
    std::span<std::byte> bytes() const { return {static_cast<std::byte*>(mapped_), static_cast<std::size_t>(size_)}; }

   private:
    void reset() noexcept;
    std::shared_ptr<VulkanDevice> device_;
    VkBuffer buffer_{};
    VkDeviceMemory memory_{};
    void* mapped_{};
    VkDeviceSize size_{};
};
}  // namespace lumaframevr
