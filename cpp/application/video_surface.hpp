#pragma once
#include "openxr_runtime.hpp"
#include "vulkan_video_image.hpp"
#include "image_reader.hpp"
#include "vulkan_video_pipeline.hpp"
#include "vulkan_render_target.hpp"
#include <filesystem>
namespace lumaframevr::application {
class VideoSurface {
   public:
    VideoSurface(std::shared_ptr<VulkanDevice> device, VkRenderPass pass, std::filesystem::path shaders);
    void upload(RetainedFrame frame);
    void upload(ImagePixels image);
    void clear();
    void draw(VkCommandBuffer command, VkExtent2D extent, const XrView& view, int eye, const VideoPresentation& presentation, float recenter, bool passthrough = false) const;
    bool ready() const { return texture_ != nullptr || image_ != nullptr; }

   private:
    std::shared_ptr<VulkanDevice> device_;
    VkRenderPass pass_;
    std::filesystem::path shaders_;
    std::unique_ptr<VulkanVideoImage> texture_;
    std::unique_ptr<VulkanSampledImage> image_;
    std::array<int, 2> image_size_{};
    std::unique_ptr<VulkanVideoPipeline> pipeline_;
};
Pose pose_of(XrPosef pose);
XrPosef xr_pose(Pose pose);
std::array<float, 4> angles_of(XrFovf fov);
}  // namespace lumaframevr::application
