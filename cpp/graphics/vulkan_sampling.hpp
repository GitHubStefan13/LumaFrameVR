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
    // The format this sampling was created for: the video paths assert that the same content reaches
    // the GPU in the same format whichever route carried it there.
    VkFormat format() const { return description_ ? description_->format : VK_FORMAT_UNDEFINED; }
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
