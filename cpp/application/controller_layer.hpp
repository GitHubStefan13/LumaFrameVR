#pragma once
#include "openxr_session.hpp"
#include "vulkan_overlay.hpp"
#include "vulkan_render_target.hpp"
#include "beam_surface.hpp"
namespace lumaframevr::application {

std::vector<const XrCompositionLayerBaseHeader*> controller_headers(std::span<const XrCompositionLayerQuad> panels, std::span<const XrCompositionLayerQuad> controllers);
class ControllerPass {
   public:
    ControllerPass(std::shared_ptr<VulkanDevice> device, VkRenderPass pass, const std::filesystem::path& shaders);
    void draw(VkCommandBuffer command, const VulkanRenderTarget& target, const BeamSurface& surface) const;

   private:
    VulkanOverlay beams_;
};
class ControllerLayer {
   public:
    ControllerLayer(OpenXrSession& session, const std::filesystem::path& shaders);
    ~ControllerLayer();
    ControllerLayer(const ControllerLayer&) = delete;
    ControllerLayer& operator=(const ControllerLayer&) = delete;
    std::vector<XrCompositionLayerQuad> draw(const std::array<XrView, 2>& views, const Scene& scene);

   private:
    void reset() noexcept;
    OpenXrSession& session_;
    XrSwapchain chain_{};
    VkExtent2D extent_{};
    std::vector<std::array<std::unique_ptr<VulkanRenderTarget>, 4>> targets_;
    std::unique_ptr<ControllerPass> pass_;
};
}  // namespace lumaframevr::application
