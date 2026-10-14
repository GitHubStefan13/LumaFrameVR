#pragma once
#include "openxr_runtime.hpp"
#include <filesystem>
#include "vulkan_render_target.hpp"
#include "vulkan_video_image.hpp"
#include "image_reader.hpp"
#include "vulkan_video_pipeline.hpp"

namespace lumaframevr {
class RenderProbe {
   public:
    explicit RenderProbe(std::string shader_directory);
    RenderProbe(std::shared_ptr<VulkanDevice> device, std::string shader_directory);
    void draw(ImagePixels image, const VideoPresentation& presentation);
    void draw(RetainedFrame frame, const VideoPresentation& presentation);
    void clear_frame();
    void save(const std::filesystem::path& path) const;

   private:
    std::shared_ptr<OpenXrRuntime> runtime_;
    std::shared_ptr<VulkanDevice> device_;
    VulkanRenderTarget target_;
    VulkanHostBuffer pixels_;
    std::unique_ptr<VulkanVideoImage> texture_;
    std::unique_ptr<VulkanSampledImage> image_;
    std::unique_ptr<VulkanVideoPipeline> pipeline_;
    std::string shader_directory_;
};
}  // namespace lumaframevr
