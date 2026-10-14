#include "controller_mesh.hpp"
#include <cstring>
#include <cmath>
#include <stdexcept>
namespace lumaframevr::application {
ControllerMesh::ControllerMesh(std::shared_ptr<VulkanDevice> device, VkRenderPass pass, const std::filesystem::path& shaders, std::span<const MeshVertex> vertices,
                               std::span<const std::uint16_t> indices, std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgba)
    : device_(std::move(device)) {
    if(vertices.empty() || vertices.size() > 65536 || indices.empty() || indices.size() % 3 || indices.size() > 3000000)
        throw std::invalid_argument("Invalid controller triangle mesh size");
    for(auto index : indices)
        if(index >= vertices.size())
            throw std::invalid_argument("Controller mesh index exceeds vertices");
    for(const auto& vertex : vertices) {
        for(auto value : vertex.position)
            if(!std::isfinite(value))
                throw std::invalid_argument("Nonfinite controller vertex");
        for(auto value : vertex.normal)
            if(!std::isfinite(value))
                throw std::invalid_argument("Nonfinite controller normal");
        for(auto value : vertex.uv)
            if(!std::isfinite(value))
                throw std::invalid_argument("Nonfinite controller UV");
    }
    count_ = static_cast<std::uint32_t>(indices.size());
    try {
        vertices_ = std::make_unique<VulkanHostBuffer>(device_, vertices.size_bytes(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        indices_ = std::make_unique<VulkanHostBuffer>(device_, indices.size_bytes(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        std::memcpy(vertices_->bytes().data(), vertices.data(), vertices.size_bytes());
        std::memcpy(indices_->bytes().data(), indices.data(), indices.size_bytes());
        texture_ = std::make_unique<VulkanSampledImage>(device_, width, height, rgba);
        create_pipeline(pass, shaders);
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
        VkDescriptorPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.maxSets = 1;
        pool.poolSizeCount = 1;
        pool.pPoolSizes = &size;
        vulkan_check(vkCreateDescriptorPool(device_->handle(), &pool, nullptr, &pool_), "create controller material pool");
        VkDescriptorSetAllocateInfo allocation{};
        allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocation.descriptorPool = pool_;
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &descriptors_;
        vulkan_check(vkAllocateDescriptorSets(device_->handle(), &allocation, &set_), "allocate controller material");
        VkDescriptorImageInfo image{texture_->sampler(), texture_->view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set_;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &image;
        vkUpdateDescriptorSets(device_->handle(), 1, &write, 0, nullptr);
    } catch(...) {
        reset();
        throw;
    }
}
void ControllerMesh::draw(VkCommandBuffer command, VkExtent2D extent, const OverlayPayload& pose) const {
    VkViewport viewport{0, static_cast<float>(extent.height), static_cast<float>(extent.width), -static_cast<float>(extent.height), 0, 1};
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(command, 0, 1, &viewport);
    vkCmdSetScissor(command, 0, 1, &scissor);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &set_, 0, nullptr);
    const auto buffer = vertices_->handle();
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(command, 0, 1, &buffer, &offset);
    vkCmdBindIndexBuffer(command, indices_->handle(), 0, VK_INDEX_TYPE_UINT16);
    vkCmdPushConstants(command, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pose), &pose);
    vkCmdDrawIndexed(command, count_, 1, 0, 0, 0);
}
void ControllerMesh::reset() noexcept {
    if(pool_)
        vkDestroyDescriptorPool(device_->handle(), pool_, nullptr);
    if(pipeline_)
        vkDestroyPipeline(device_->handle(), pipeline_, nullptr);
    if(layout_)
        vkDestroyPipelineLayout(device_->handle(), layout_, nullptr);
    if(descriptors_)
        vkDestroyDescriptorSetLayout(device_->handle(), descriptors_, nullptr);
}
ControllerMesh::~ControllerMesh() {
    reset();
}
}  // namespace lumaframevr::application
