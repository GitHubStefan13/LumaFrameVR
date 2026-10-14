#pragma once
#include "vulkan_sampling.hpp"
#include <span>
namespace lumaframevr {
class VulkanSampledImage {
   public:
    VulkanSampledImage(std::shared_ptr<VulkanDevice> device, std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgba, std::shared_ptr<VulkanSampling> reuse = {});
    ~VulkanSampledImage();
    VulkanSampledImage(const VulkanSampledImage&) = delete;
    VulkanSampledImage& operator=(const VulkanSampledImage&) = delete;
    VkImageView view() const { return view_; }
    VkSampler sampler() const { return sampling_->handle(); }
    const std::shared_ptr<VulkanSampling>& sampling() const { return sampling_; }
    VkImage image() const { return image_; }
    std::uint32_t descriptor_count() const { return descriptor_count_; }

   protected:
    explicit VulkanSampledImage(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanSampling> reuse = {}) : device_(std::move(device)), sampling_(std::move(reuse)) {}
    void create_image(VkFormat format, std::uint32_t width, std::uint32_t height, VkImageUsageFlags usage);
    void create_sampling(VkFormat format);
    void upload_rgba(std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgba);
    void reset() noexcept;
    std::shared_ptr<VulkanDevice> device_;
    VkImage image_{};
    VkDeviceMemory memory_{};
    VkImageView view_{};
    std::shared_ptr<VulkanSampling> sampling_;
    std::uint32_t descriptor_count_{1};
};
}  // namespace lumaframevr
