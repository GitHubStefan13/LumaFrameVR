#include "controller_layer.hpp"
#include "video_surface.hpp"
namespace lumaframevr::application {
std::vector<const XrCompositionLayerBaseHeader*> controller_headers(std::span<const XrCompositionLayerQuad> panels, std::span<const XrCompositionLayerQuad> controllers) {
    std::vector<const XrCompositionLayerBaseHeader*> headers;
    headers.reserve(panels.size() + controllers.size());
    for(const auto& panel : panels) headers.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&panel));
    for(const auto& controller : controllers) headers.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&controller));
    return headers;
}
ControllerPass::ControllerPass(std::shared_ptr<VulkanDevice> device, VkRenderPass pass, const std::filesystem::path& shaders) : beams_(std::move(device), pass, shaders, "beam_strip.frag.spv") {}
void ControllerPass::draw(VkCommandBuffer command, const VulkanRenderTarget& target, const BeamSurface& surface) const {
    target.begin(command, 0);
    beams_.draw(command, target.extent(), std::span<const OverlayPayload>(&surface.raster, 1));
    target.end(command);
}
}  // namespace lumaframevr::application
