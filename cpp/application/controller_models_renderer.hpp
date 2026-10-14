#pragma once
#include "controller_mesh.hpp"
#include "model_runtime.hpp"
#include "openxr_session.hpp"
namespace lumaframevr::application {
struct MeshAsset {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint16_t> indices;
    std::vector<std::uint8_t> rgba;
    std::uint32_t width{}, height{};
    Pose grip_from_model;
};
class ControllerModelsRenderer {
   public:
    ControllerModelsRenderer(std::shared_ptr<VulkanDevice> device, VkRenderPass pass, std::filesystem::path shaders);
    void install(unsigned hand, std::span<const MeshAsset> assets);
    void synchronize(const RuntimeModelSnapshot& snapshot);
    void draw(VkCommandBuffer command, VkExtent2D extent, const XrView& view, const std::array<std::optional<Pose>, 2>& grips) const;

   private:
    struct Part {
        std::unique_ptr<ControllerMesh> mesh;
        Pose transform;
    };
    std::shared_ptr<VulkanDevice> device_;
    VkRenderPass pass_;
    std::filesystem::path shaders_;
    std::array<std::vector<Part>, 2> parts_;
    std::array<std::shared_ptr<const RuntimeControllerModel>, 2> installed_;
};
}  // namespace lumaframevr::application
