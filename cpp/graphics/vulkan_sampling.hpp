#pragma once
#include "vulkan_device.hpp"
#include <optional>

namespace lumaframevr {
class VulkanSampling {
   public:
    explicit VulkanSampling(std::shared_ptr<VulkanDevice> device, const VkSamplerYcbcrConversionCreateInfo* description = nullptr);
    ~VulkanSampling();
    VulkanSampling(const VulkanSampling&) = delete;
    VulkanSampling& operator=(const VulkanSampling&) = delete;
    VkSampler handle() const { return sampler_; }
    VkSamplerYcbcrConversion conversion() const { return conversion_; }
    bool matches(const std::shared_ptr<VulkanDevice>& device, const VkSamplerYcbcrConversionCreateInfo* description) const;
    bool compatible(const VulkanSampling& other) const;

   private:
    void reset() noexcept;
    std::shared_ptr<VulkanDevice> device_;
    std::optional<VkSamplerYcbcrConversionCreateInfo> description_;
    VkSampler sampler_{};
    VkSamplerYcbcrConversion conversion_{};
};
}  // namespace lumaframevr
