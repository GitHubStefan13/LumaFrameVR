#include "vulkan_compute_kernel.hpp"
#include "vulkan_shader_module.hpp"
#include <vector>
#include <stdexcept>
namespace lumaframevr {
VulkanComputeKernel::VulkanComputeKernel(std::shared_ptr<VulkanDevice> device, const std::string& path) : device_(std::move(device)) {
    try {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        VkDescriptorSetLayoutCreateInfo descriptor{};
        descriptor.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor.bindingCount = 1;
        descriptor.pBindings = &binding;
        vulkan_check(vkCreateDescriptorSetLayout(device_->handle(), &descriptor, nullptr, &descriptor_layout_), "create compute descriptor layout");
        VkPipelineLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &descriptor_layout_;
        vulkan_check(vkCreatePipelineLayout(device_->handle(), &layout, nullptr, &pipeline_layout_), "create compute pipeline layout");
        ScopedShaderModule module(device_->handle(), path, "create compute shader module");
        VkComputePipelineCreateInfo pipeline{};
        pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline.stage.module = module.handle();
        pipeline.stage.pName = "main";
        pipeline.layout = pipeline_layout_;
        vulkan_check(vkCreateComputePipelines(device_->handle(), VK_NULL_HANDLE, 1, &pipeline, nullptr, &pipeline_), "create compute pipeline");
        module.reset();
        const VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
        VkDescriptorPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.maxSets = 1;
        pool.poolSizeCount = 1;
        pool.pPoolSizes = &pool_size;
        vulkan_check(vkCreateDescriptorPool(device_->handle(), &pool, nullptr, &descriptor_pool_), "create compute descriptor pool");
    } catch(...) {
        reset();
        throw;
    }
}
void VulkanComputeKernel::dispatch(VkCommandBuffer command, const VulkanHostBuffer& output) {
    vkResetDescriptorPool(device_->handle(), descriptor_pool_, 0);
    VkDescriptorSetAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool = descriptor_pool_;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &descriptor_layout_;
    VkDescriptorSet set{};
    vulkan_check(vkAllocateDescriptorSets(device_->handle(), &allocate, &set), "allocate compute descriptors");
    const VkDescriptorBufferInfo buffer{output.handle(), 0, output.size()};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &buffer;
    vkUpdateDescriptorSets(device_->handle(), 1, &write, 0, nullptr);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_, 0, 1, &set, 0, nullptr);
    vkCmdDispatch(command, 1, 1, 1);
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}
void VulkanComputeKernel::reset() noexcept {
    if(descriptor_pool_)
        vkDestroyDescriptorPool(device_->handle(), descriptor_pool_, nullptr);
    if(pipeline_)
        vkDestroyPipeline(device_->handle(), pipeline_, nullptr);
    if(pipeline_layout_)
        vkDestroyPipelineLayout(device_->handle(), pipeline_layout_, nullptr);
    if(descriptor_layout_)
        vkDestroyDescriptorSetLayout(device_->handle(), descriptor_layout_, nullptr);
}
VulkanComputeKernel::~VulkanComputeKernel() {
    reset();
}
}  // namespace lumaframevr
