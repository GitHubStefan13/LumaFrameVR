#pragma once
#include "application_scene.hpp"
#include "application_assets.hpp"
#include "vulkan_host_buffer.hpp"
#include "vulkan_sampled_image.hpp"
namespace lumaframevr::application {
struct MeshVertex {
    std::array<float, 3> position, normal;
    std::array<float, 2> uv;
};
static_assert(sizeof(MeshVertex) == 32);
class ControllerMesh {
   public:
    ControllerMesh(std::shared_ptr<VulkanDevice> device, VkRenderPass pass, const std::filesystem::path& shaders, std::span<const MeshVertex> vertices, std::span<const std::uint16_t> indices,
                   std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgba);
    ~ControllerMesh();
    ControllerMesh(const ControllerMesh&) = delete;
    ControllerMesh& operator=(const ControllerMesh&) = delete;
    void draw(VkCommandBuffer command, VkExtent2D extent, const OverlayPayload& pose) const;

   private:
    void create_pipeline(VkRenderPass pass, const std::filesystem::path& shaders);
    void reset() noexcept;
    std::shared_ptr<VulkanDevice> device_;
    std::unique_ptr<VulkanHostBuffer> vertices_, indices_;
    std::unique_ptr<VulkanSampledImage> texture_;
    VkDescriptorSetLayout descriptors_{};
    VkDescriptorPool pool_{};
    VkDescriptorSet set_{};
    VkPipelineLayout layout_{};
    VkPipeline pipeline_{};
    std::uint32_t count_{};
};
}  // namespace lumaframevr::application
