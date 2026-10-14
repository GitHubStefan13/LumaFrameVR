#pragma once
#include "application_scene.hpp"
#include "openxr_session.hpp"
namespace lumaframevr::application {
struct BeamSurface {
    Pose pose;
    std::array<float, 2> size;
    OverlayPayload raster;
};
std::optional<BeamSurface> beam_surface(const OverlayPayload& beam, Pose viewer);
std::optional<BeamSurface> beam_reticle(const OverlayPayload& beam, Pose viewer);
std::vector<XrCompositionLayerQuad> beam_quads(XrSpace space, XrSwapchain chain, VkExtent2D extent, std::span<const BeamSurface> surfaces);
}  // namespace lumaframevr::application
