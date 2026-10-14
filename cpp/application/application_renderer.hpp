#pragma once
#include "openxr_session.hpp"
#include "video_surface.hpp"
#include "controller_models_renderer.hpp"
#include <map>
namespace lumaframevr::application {
class Renderer {
   public:
    Renderer(OpenXrSession& session, const std::filesystem::path& shaders);
    void upload(RetainedFrame frame);
    void upload(ImagePixels image) { video_->upload(std::move(image)); }
    void clear();
    void passthrough(bool enabled) { passthrough_ = enabled; }
    void update_controller_models(const RuntimeModelSnapshot& snapshot);
    void draw(VkImage image, const std::array<XrView, 2>& views, const Scene& scene, const VideoPresentation& presentation, float recenter);
    void snapshot(const std::filesystem::path& path, const XrView& view, const Scene& scene, const VideoPresentation& presentation, float recenter);
    bool ready() const { return video_->ready(); }

   private:
    void eye(VkCommandBuffer command, const VulkanRenderTarget& target, const XrView& view, int eye, const Scene& scene, const VideoPresentation& presentation, float recenter,
             bool controllers = true);
    OpenXrSession& session_;
    std::map<VkImage, std::array<std::unique_ptr<VulkanRenderTarget>, 2>> targets_;
    std::unique_ptr<VideoSurface> video_;
    std::unique_ptr<ControllerModelsRenderer> models_;
    bool passthrough_{};
};
}  // namespace lumaframevr::application
