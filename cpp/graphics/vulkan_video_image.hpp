#pragma once
#include "vulkan_sampled_image.hpp"
#include "retained_frame.hpp"
#include "software_pixel_payload.hpp"
namespace lumaframevr {
VkSamplerYcbcrConversionCreateInfo video_sampling_description(VkFormat format, int colorspace, int range, const FrameColor& color, VkFormatFeatureFlags features);
VkFormat dma_buf_format(std::uint32_t fourcc, int bits);
std::array<float, 4> dma_buf_crop(int width, int height, std::array<int, 4> crop);
class VulkanVideoImage final : public VulkanSampledImage {
   public:
    VulkanVideoImage(std::shared_ptr<VulkanDevice> device, RetainedFrame frame, std::shared_ptr<VulkanSampling> reuse = {});
    ~VulkanVideoImage();
    void release_foreign();
    const RetainedFrame& frame() const { return frame_; }
    const std::array<float, 4>& crop() const { return crop_; }

   private:
    void create_conversion(VkFormat format, VkFormatFeatureFlags features);
    void import_dma();
    void upload_yuv(const SoftwarePixelPayload& pixels);
    RetainedFrame frame_;
    bool foreign_owned_{};
    std::array<float, 4> crop_{0, 0, 1, 1};
};
}  // namespace lumaframevr
