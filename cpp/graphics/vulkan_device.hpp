#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
namespace lumaframevr {
void vulkan_check(VkResult result, const char* operation);
class VulkanDevice {
   public:
    static std::shared_ptr<VulkanDevice> create();
    static std::shared_ptr<VulkanDevice> adopt(VkInstance instance, VkPhysicalDevice physical, VkDevice device, std::uint32_t family);
    VkInstance instance() const { return instance_; }
    ~VulkanDevice();
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VkDevice handle() const { return device_; }
    VkPhysicalDevice physical() const { return physical_; }
    VkQueue queue() const { return queue_; }
    std::uint32_t queue_family() const { return queue_family_; }
    std::string name() const;
    std::uint32_t memory_type(std::uint32_t mask, VkMemoryPropertyFlags flags) const;

   private:
    VulkanDevice() = default;
    VkInstance instance_{};
    VkPhysicalDevice physical_{};
    VkDevice device_{};
    VkQueue queue_{};
    std::uint32_t queue_family_{};
};
}  // namespace lumaframevr
