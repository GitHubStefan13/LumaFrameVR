#pragma once
#include "vulkan_sampled_image.hpp"
#include "video_view_parameters.hpp"
namespace lumaframevr {
struct VideoShaderParameters {
    VideoViewParameters view{};
    std::array<float, 4> crop{};
};
static_assert(sizeof(VideoShaderParameters) == 128);
static_assert(offsetof(VideoShaderParameters, crop) == 112);
class VulkanVideoPipeline {
   public:
    VulkanVideoPipeline(std::shared_ptr<VulkanDevice> device, VkRenderPass pass, const VulkanSampledImage& texture, const std::string& shader_directory);
    ~VulkanVideoPipeline();
    VulkanVideoPipeline(const VulkanVideoPipeline&) = delete;
    VulkanVideoPipeline& operator=(const VulkanVideoPipeline&) = delete;
    void bind(const VulkanSampledImage& texture);
    const std::shared_ptr<VulkanSampling>& sampling() const { return sampling_; }
    void draw(VkCommandBuffer command, const VideoShaderParameters& parameters, VkExtent2D extent) const;

   private:
    void reset() noexcept;
    std::shared_ptr<VulkanDevice> device_;
    std::shared_ptr<VulkanSampling> sampling_;
    VkDescriptorSetLayout descriptors_{};
    VkPipelineLayout layout_{};
    VkPipeline pipeline_{};
    VkDescriptorPool pool_{};
    VkDescriptorSet set_{};
};
}  // namespace lumaframevr
