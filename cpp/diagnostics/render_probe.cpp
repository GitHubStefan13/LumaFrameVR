#include "render_probe.hpp"
#include "image_view_parameters.hpp"
#include "rgba_readback.hpp"
#include "vulkan_submission.hpp"
#include <filesystem>
#include <fstream>
#include <numbers>
#include <stdexcept>

namespace lumaframevr {
RenderProbe::RenderProbe(std::string shader_directory)
    : runtime_(OpenXrRuntime::create()),
      device_(runtime_->create_graphics()),
      target_(device_, 1024, 1024, VK_FORMAT_R8G8B8A8_SRGB),
      pixels_(device_, 1024 * 1024 * 4),
      shader_directory_(std::move(shader_directory)) {}
RenderProbe::RenderProbe(std::shared_ptr<VulkanDevice> device, std::string shader_directory)
    : device_(std::move(device)), target_(device_, 1024, 1024, VK_FORMAT_R8G8B8A8_SRGB), pixels_(device_, 1024 * 1024 * 4), shader_directory_(std::move(shader_directory)) {}
void RenderProbe::draw(ImagePixels image, const VideoPresentation& presentation) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device_->physical(), &properties);
    if(image.width <= 0 || image.height <= 0 || static_cast<unsigned>(image.width) > properties.limits.maxImageDimension2D ||
       static_cast<unsigned>(image.height) > properties.limits.maxImageDimension2D)
        throw std::invalid_argument("Image exceeds the GPU texture dimension limit");
    auto next = std::make_unique<VulkanSampledImage>(device_, image.width, image.height, image.rgba, pipeline_ ? pipeline_->sampling() : nullptr);
    if(!pipeline_ || !pipeline_->sampling()->compatible(*next->sampling()))
        pipeline_ = std::make_unique<VulkanVideoPipeline>(device_, target_.render_pass(), *next, shader_directory_);
    else
        pipeline_->bind(*next);
    if(texture_)
        texture_->release_foreign();
    texture_.reset();
    image_ = std::move(next);
    constexpr float angle = std::numbers::pi_v<float> / 4;
    VideoShaderParameters parameters{};
    parameters.view = image_view_parameters(image.width, image.height, {}, {-angle, angle, -angle, angle}, 0, presentation, 0);
    parameters.crop = {0, 0, 1, 1};
    VulkanSubmission submission(device_);
    target_.begin(submission.command());
    pipeline_->draw(submission.command(), parameters, target_.extent());
    target_.end(submission.command());
    target_.read(submission.command(), pixels_);
    submission.submit();
    submission.wait();
}
void RenderProbe::draw(RetainedFrame frame, const VideoPresentation& presentation) {
    auto next = std::make_unique<VulkanVideoImage>(device_, std::move(frame), pipeline_ ? pipeline_->sampling() : nullptr);
    if(!pipeline_ || pipeline_->sampling() != next->sampling())
        pipeline_ = std::make_unique<VulkanVideoPipeline>(device_, target_.render_pass(), *next, shader_directory_);
    else
        pipeline_->bind(*next);
    if(texture_)
        texture_->release_foreign();
    texture_ = std::move(next);
    image_.reset();
    constexpr float angle = std::numbers::pi_v<float> / 4;
    VideoShaderParameters parameters{};
    const auto& picture = texture_->frame();
    parameters.view = view_parameters(picture.width(), picture.height(), picture.sample_aspect_ratio(), {}, {-angle, angle, -angle, angle}, 0, presentation, 0);
    const auto color = picture.color().shader_controls();
    parameters.view.framing[3] = color[0];
    parameters.view.stereo_offset[2] = color[1];
    parameters.view.stereo_offset[3] = color[2];
    parameters.crop = texture_->crop();
    VulkanSubmission submission(device_);
    target_.begin(submission.command());
    pipeline_->draw(submission.command(), parameters, target_.extent());
    target_.end(submission.command());
    target_.read(submission.command(), pixels_);
    submission.submit();
    submission.wait();
}
void RenderProbe::clear_frame() {
    if(texture_)
        texture_->release_foreign();
    pipeline_.reset();
    texture_.reset();
    image_.reset();
}
void RenderProbe::save(const std::filesystem::path& path) const {
    if(!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    ui::canvas_from_readback(1024, 1024, pixels_.bytes()).save_ppm(path);
}
}  // namespace lumaframevr
