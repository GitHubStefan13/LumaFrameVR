#include "vulkan_sampling.hpp"
#include <stdexcept>

namespace lumaframevr {
VulkanSampling::VulkanSampling(std::shared_ptr<VulkanDevice> device, const VkSamplerYcbcrConversionCreateInfo* description) : device_(std::move(device)) {
    if(!device_)
        throw std::invalid_argument("Sampler requires a Vulkan device");
    if(description && description->pNext)
        throw std::invalid_argument("YCbCr sampler description must not retain an external chain");
    try {
        if(description) {
            description_ = *description;
            vulkan_check(vkCreateSamplerYcbcrConversion(device_->handle(), description, nullptr, &conversion_), "create source-color YCbCr conversion");
        }
        VkSamplerYcbcrConversionInfo conversion{};
        conversion.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO;
        conversion.conversion = conversion_;
        VkSamplerCreateInfo sampler{};
        sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler.pNext = conversion_ ? &conversion : nullptr;
        sampler.magFilter = sampler.minFilter = description ? description->chromaFilter : VK_FILTER_LINEAR;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vulkan_check(vkCreateSampler(device_->handle(), &sampler, nullptr, &sampler_), "create image sampler");
    } catch(...) {
        reset();
        throw;
    }
}
bool VulkanSampling::matches(const std::shared_ptr<VulkanDevice>& device, const VkSamplerYcbcrConversionCreateInfo* description) const {
    if(device_ != device || description_.has_value() != (description != nullptr))
        return false;
    if(!description)
        return true;
    const auto& own = *description_;
    return !description->pNext && own.format == description->format && own.ycbcrModel == description->ycbcrModel && own.ycbcrRange == description->ycbcrRange &&
           own.xChromaOffset == description->xChromaOffset && own.yChromaOffset == description->yChromaOffset && own.chromaFilter == description->chromaFilter &&
           own.forceExplicitReconstruction == description->forceExplicitReconstruction && own.components.r == description->components.r && own.components.g == description->components.g &&
           own.components.b == description->components.b && own.components.a == description->components.a;
}
bool VulkanSampling::compatible(const VulkanSampling& other) const {
    return matches(other.device_, other.description_ ? &*other.description_ : nullptr);
}
void VulkanSampling::reset() noexcept {
    if(sampler_)
        vkDestroySampler(device_->handle(), sampler_, nullptr);
    if(conversion_)
        vkDestroySamplerYcbcrConversion(device_->handle(), conversion_, nullptr);
    sampler_ = {};
    conversion_ = {};
}
VulkanSampling::~VulkanSampling() {
    reset();
}
}  // namespace lumaframevr
