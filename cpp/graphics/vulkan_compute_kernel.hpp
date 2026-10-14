#pragma once
#include "vulkan_host_buffer.hpp"
#include "vulkan_submission.hpp"
namespace lumaframevr {
class VulkanComputeKernel {
   public:
    VulkanComputeKernel(std::shared_ptr<VulkanDevice> device, const std::string& path);
    ~VulkanComputeKernel();
    VulkanComputeKernel(const VulkanComputeKernel&) = delete;
    VulkanComputeKernel& operator=(const VulkanComputeKernel&) = delete;
    void dispatch(VkCommandBuffer command, const VulkanHostBuffer& output);

   private:
    void reset() noexcept;
    std::shared_ptr<VulkanDevice> device_;
    VkDescriptorSetLayout descriptor_layout_{};
    VkPipelineLayout pipeline_layout_{};
    VkPipeline pipeline_{};
    VkDescriptorPool descriptor_pool_{};
};
}  // namespace lumaframevr
