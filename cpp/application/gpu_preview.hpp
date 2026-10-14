#pragma once
#include "openxr_session.hpp"
#include "application_scene.hpp"
#include "video_surface.hpp"
namespace lumaframevr::application {
class GpuPreview {
   public:
    GpuPreview(OpenXrSession& session, const std::filesystem::path& shaders);
    ~GpuPreview();
    GpuPreview(const GpuPreview&) = delete;
    GpuPreview& operator=(const GpuPreview&) = delete;
    void update(Controller& app, double now);
    void clear();
    std::vector<XrCompositionLayerQuad> layers(const Scene& scene, const Controller& app) const;

   private:
    void render(const VideoPresentation& presentation);
    void reset() noexcept;
    OpenXrSession& session_;
    XrSwapchain chain_{};
    std::vector<XrSwapchainImageVulkan2KHR> images_;
    std::vector<std::unique_ptr<VulkanRenderTarget>> targets_;
    std::unique_ptr<VideoSurface> video_;
    std::filesystem::path selection_;
    bool shown_{}, explorer_{};
};
}  // namespace lumaframevr::application
