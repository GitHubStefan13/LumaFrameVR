#include "controller_models_renderer.hpp"
#include "video_surface.hpp"
#include <stdexcept>
namespace lumaframevr::application {
ControllerModelsRenderer::ControllerModelsRenderer(std::shared_ptr<VulkanDevice> device, VkRenderPass pass, std::filesystem::path shaders)
    : device_(std::move(device)), pass_(pass), shaders_(std::move(shaders)) {}
void ControllerModelsRenderer::install(unsigned hand, std::span<const MeshAsset> assets) {
    if(hand >= parts_.size())
        throw std::invalid_argument("Invalid controller model hand");
    if(assets.size() > 128)
        throw std::invalid_argument("Too many controller model parts");
    std::vector<Part> ready;
    for(const auto& asset : assets) {
        ready.push_back({std::make_unique<ControllerMesh>(device_, pass_, shaders_, asset.vertices, asset.indices, asset.width, asset.height, asset.rgba), asset.grip_from_model});
    }
    parts_[hand] = std::move(ready);
}
void ControllerModelsRenderer::synchronize(const RuntimeModelSnapshot& snapshot) {
    for(unsigned hand = 0; hand < 2; ++hand) {
        const auto& source = snapshot.hands[hand];
        const auto usable = source.status == ModelLoadStatus::Ready && source.model && source.model->grip_aligned;
        const auto model = usable ? source.model : nullptr;
        if(installed_[hand] == model)
            continue;
        installed_[hand] = model;
        parts_[hand].clear();
        if(!model)
            continue;
        std::vector<MeshAsset> assets;
        for(const auto& part : model->parts) {
            if(!part.visible)
                continue;
            if(auto issue = validate_controller_model(part); !issue.empty())
                throw std::runtime_error("Runtime controller mesh: " + issue);
            MeshAsset asset;
            asset.indices = part.indices;
            asset.rgba = part.rgba;
            asset.width = part.width;
            asset.height = part.height;
            for(const auto& vertex : part.vertices) asset.vertices.push_back({vertex.position, vertex.normal, vertex.uv});
            assets.push_back(std::move(asset));
        }
        install(hand, assets);
    }
}
void ControllerModelsRenderer::draw(VkCommandBuffer command, VkExtent2D extent, const XrView& view, const std::array<std::optional<Pose>, 2>& grips) const {
    const auto eye = pose_of(view.pose);
    auto fov = angles_of(view.fov);
    for(auto& angle : fov) angle = std::tan(angle);
    for(unsigned hand = 0; hand < parts_.size(); ++hand) {
        if(!grips[hand])
            continue;
        for(const auto& part : parts_[hand]) {
            const auto pose = compose(*grips[hand], part.transform);
            OverlayPayload payload;
            for(unsigned axis = 0; axis < 3; ++axis) payload.values[0][axis] = pose.position[axis] - eye.position[axis];
            payload.values[1] = pose.rotation;
            payload.values[4] = conjugate(eye.rotation);
            payload.values[5] = fov;
            part.mesh->draw(command, extent, payload);
        }
    }
}
}  // namespace lumaframevr::application
