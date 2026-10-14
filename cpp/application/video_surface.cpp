#include "openxr_runtime.hpp"
#include "video_surface.hpp"
#include "image_view_parameters.hpp"
namespace lumaframevr::application {
Pose pose_of(XrPosef pose) {
    return {{pose.position.x, pose.position.y, pose.position.z}, {pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w}};
}
XrPosef xr_pose(Pose pose) {
    return {{pose.rotation[0], pose.rotation[1], pose.rotation[2], pose.rotation[3]}, {pose.position[0], pose.position[1], pose.position[2]}};
}
std::array<float, 4> angles_of(XrFovf fov) {
    return {fov.angleLeft, fov.angleRight, fov.angleDown, fov.angleUp};
}
VideoSurface::VideoSurface(std::shared_ptr<VulkanDevice> device, VkRenderPass pass, std::filesystem::path shaders) : device_(std::move(device)), pass_(pass), shaders_(std::move(shaders)) {}
void VideoSurface::upload(RetainedFrame frame) {
    auto next = std::make_unique<VulkanVideoImage>(device_, std::move(frame), pipeline_ ? pipeline_->sampling() : nullptr);
    if(!pipeline_ || !pipeline_->sampling()->compatible(*next->sampling()))
        pipeline_ = std::make_unique<VulkanVideoPipeline>(device_, pass_, *next, shaders_.string());
    else
        pipeline_->bind(*next);
    texture_ = std::move(next);
    image_.reset();
}
void VideoSurface::upload(ImagePixels image) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device_->physical(), &properties);
    const auto limit = properties.limits.maxImageDimension2D;
    if(image.width <= 0 || image.height <= 0 || static_cast<unsigned>(image.width) > limit || static_cast<unsigned>(image.height) > limit)
        throw std::invalid_argument("Image exceeds the GPU texture dimension limit: " + std::to_string(limit));
    auto next = std::make_unique<VulkanSampledImage>(device_, image.width, image.height, image.rgba, pipeline_ ? pipeline_->sampling() : nullptr);
    if(!pipeline_ || !pipeline_->sampling()->compatible(*next->sampling()))
        pipeline_ = std::make_unique<VulkanVideoPipeline>(device_, pass_, *next, shaders_.string());
    else
        pipeline_->bind(*next);
    if(texture_)
        texture_->release_foreign();
    texture_.reset();
    image_size_ = {image.width, image.height};
    image_ = std::move(next);
}
void VideoSurface::clear() {
    pipeline_.reset();
    if(texture_)
        texture_->release_foreign();
    texture_.reset();
    image_.reset();
}
void VideoSurface::draw(VkCommandBuffer command, VkExtent2D extent, const XrView& view, int eye, const VideoPresentation& presentation, float recenter, bool passthrough) const {
    if(!ready())
        return;
    if(image_) {
        VideoShaderParameters parameters;
        parameters.view = image_view_parameters(image_size_[0], image_size_[1], pose_of(view.pose), angles_of(view.fov), eye, presentation, recenter);
        parameters.view.eye |= passthrough ? 2 : 0;
        parameters.crop = {0, 0, 1, 1};
        pipeline_->draw(command, parameters, extent);
        return;
    }
    const auto& frame = texture_->frame();
    VideoShaderParameters parameters;
    parameters.view = view_parameters(frame.width(), frame.height(), frame.sample_aspect_ratio(), pose_of(view.pose), angles_of(view.fov), eye, presentation, recenter);
    parameters.view.eye = eye | (passthrough ? 2 : 0);
    parameters.crop = texture_->crop();
    const auto color = frame.color().shader_controls();
    parameters.view.framing[3] = color[0];
    parameters.view.stereo_offset[2] = color[1];
    parameters.view.stereo_offset[3] = color[2];
    pipeline_->draw(command, parameters, extent);
}
}  // namespace lumaframevr::application
