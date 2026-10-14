#include "vulkan_video_pipeline.hpp"
#include "vulkan_shader_module.hpp"
#include <vector>
#include <stdexcept>
namespace lumaframevr {
VulkanVideoPipeline::VulkanVideoPipeline(std::shared_ptr<VulkanDevice> device, VkRenderPass pass, const VulkanSampledImage& texture, const std::string& directory)
    : device_(std::move(device)), sampling_(texture.sampling()) {
    try {
        VkSampler sampler = texture.sampler();
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        binding.pImmutableSamplers = &sampler;
        VkDescriptorSetLayoutCreateInfo descriptors{};
        descriptors.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptors.bindingCount = 1;
        descriptors.pBindings = &binding;
        vulkan_check(vkCreateDescriptorSetLayout(device_->handle(), &descriptors, nullptr, &descriptors_), "create immutable video descriptor");
        VkPushConstantRange constants{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VideoShaderParameters)};
        VkPipelineLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &descriptors_;
        layout.pushConstantRangeCount = 1;
        layout.pPushConstantRanges = &constants;
        vulkan_check(vkCreatePipelineLayout(device_->handle(), &layout, nullptr, &layout_), "create video pipeline layout");
        ScopedShaderModule vertex(device_->handle(), directory + "/quad.vert.spv", "load video vertex shader");
        ScopedShaderModule fragment(device_->handle(), directory + "/video.frag.spv", "load video fragment shader");
        std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
        for(auto& stage : stages) {
            stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stage.pName = "main";
        }
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertex.handle();
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragment.handle();
        VkPipelineVertexInputStateCreateInfo input{};
        input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState attachment{};
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &attachment;
        std::array<VkDynamicState, 2> states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = states.data();
        VkPipelineDepthStencilStateCreateInfo depth{};
        depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        VkGraphicsPipelineCreateInfo pipeline{};
        pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline.stageCount = 2;
        pipeline.pStages = stages.data();
        pipeline.pVertexInputState = &input;
        pipeline.pInputAssemblyState = &assembly;
        pipeline.pDepthStencilState = &depth;
        pipeline.pViewportState = &viewport;
        pipeline.pRasterizationState = &raster;
        pipeline.pMultisampleState = &multisample;
        pipeline.pColorBlendState = &blend;
        pipeline.pDynamicState = &dynamic;
        pipeline.layout = layout_;
        pipeline.renderPass = pass;
        vulkan_check(vkCreateGraphicsPipelines(device_->handle(), VK_NULL_HANDLE, 1, &pipeline, nullptr, &pipeline_), "create original GLSL video pipeline");
        vertex.reset();
        fragment.reset();
        VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texture.descriptor_count()};
        VkDescriptorPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.maxSets = 1;
        pool.poolSizeCount = 1;
        pool.pPoolSizes = &pool_size;
        vulkan_check(vkCreateDescriptorPool(device_->handle(), &pool, nullptr, &pool_), "create video descriptor pool");
        VkDescriptorSetAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate.descriptorPool = pool_;
        allocate.descriptorSetCount = 1;
        allocate.pSetLayouts = &descriptors_;
        vulkan_check(vkAllocateDescriptorSets(device_->handle(), &allocate, &set_), "allocate video descriptors");
        bind(texture);
    } catch(...) {
        reset();
        throw;
    }
}
void VulkanVideoPipeline::bind(const VulkanSampledImage& texture) {
    if(!sampling_->compatible(*texture.sampling()))
        throw std::invalid_argument("Video descriptor requires a compatible immutable sampler");
    VkDescriptorImageInfo image{sampling_->handle(), texture.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set_;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image;
    vkUpdateDescriptorSets(device_->handle(), 1, &write, 0, nullptr);
}
void VulkanVideoPipeline::draw(VkCommandBuffer command, const VideoShaderParameters& parameters, VkExtent2D extent) const {
    VkViewport viewport{0, static_cast<float>(extent.height), static_cast<float>(extent.width), -static_cast<float>(extent.height), 0, 1};
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(command, 0, 1, &viewport);
    vkCmdSetScissor(command, 0, 1, &scissor);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &set_, 0, nullptr);
    vkCmdPushConstants(command, layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(parameters), &parameters);
    vkCmdDraw(command, 3, 1, 0, 0);
}
void VulkanVideoPipeline::reset() noexcept {
    if(pool_)
        vkDestroyDescriptorPool(device_->handle(), pool_, nullptr);
    if(pipeline_)
        vkDestroyPipeline(device_->handle(), pipeline_, nullptr);
    if(layout_)
        vkDestroyPipelineLayout(device_->handle(), layout_, nullptr);
    if(descriptors_)
        vkDestroyDescriptorSetLayout(device_->handle(), descriptors_, nullptr);
}
VulkanVideoPipeline::~VulkanVideoPipeline() {
    reset();
}
}  // namespace lumaframevr
