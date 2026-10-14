#include "application_renderer.hpp"
#include "vulkan_submission.hpp"
#include "rgba_readback.hpp"
#include "openxr_swapchain.hpp"
namespace lumaframevr::application {
Renderer::Renderer(OpenXrSession& session, const std::filesystem::path& shaders) : session_(session) {
    require_swapchain_images(session.images(), "create stereo renderer");
    for(const auto& image : session.images()) {
        auto& eyes = targets_[image.image];
        for(unsigned eye = 0; eye < 2; ++eye) eyes[eye] = std::make_unique<VulkanRenderTarget>(session.graphics(), image.image, session.extent(), session.format(), eye, true);
    }
    const auto pass = targets_.begin()->second[0]->render_pass();
    video_ = std::make_unique<VideoSurface>(session.graphics(), pass, shaders);
    models_ = std::make_unique<ControllerModelsRenderer>(session.graphics(), pass, shaders);
}
void Renderer::upload(RetainedFrame frame) {
    video_->upload(std::move(frame));
}
void Renderer::clear() {
    video_->clear();
}
void Renderer::update_controller_models(const RuntimeModelSnapshot& snapshot) {
    models_->synchronize(snapshot);
}
void Renderer::eye(VkCommandBuffer command, const VulkanRenderTarget& target, const XrView& view, int eye, const Scene& scene, const VideoPresentation& presentation, float recenter,
                   bool controllers) {
    target.begin(command, passthrough_ ? 0 : 1);
    video_->draw(command, target.extent(), view, eye, presentation, recenter, passthrough_);
    if(controllers)
        models_->draw(command, target.extent(), view, scene.grip_poses());
    target.end(command);
}
void Renderer::draw(VkImage image, const std::array<XrView, 2>& views, const Scene& scene, const VideoPresentation& presentation, float recenter) {
    VulkanSubmission submission(session_.graphics());
    for(unsigned index = 0; index < 2; ++index) eye(submission.command(), *targets_.at(image)[index], views[index], static_cast<int>(index), scene, presentation, recenter, true);
    submission.submit();
    submission.wait();
}
void Renderer::snapshot(const std::filesystem::path& path, const XrView& view, const Scene& scene, const VideoPresentation& presentation, float recenter) {
    const auto extent = session_.extent();
    VulkanRenderTarget target(session_.graphics(), extent.width, extent.height, session_.format(), true);
    VulkanHostBuffer pixels(session_.graphics(), static_cast<VkDeviceSize>(extent.width) * extent.height * 4);
    VulkanSubmission submission(session_.graphics());
    eye(submission.command(), target, view, 0, scene, presentation, recenter);
    target.read(submission.command(), pixels);
    submission.submit();
    submission.wait();
    const auto canvas = ui::canvas_from_readback(static_cast<int>(extent.width), static_cast<int>(extent.height), pixels.bytes(), session_.format() == VK_FORMAT_B8G8R8A8_SRGB);
    if(!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    canvas.save_ppm(path);
}
}  // namespace lumaframevr::application
