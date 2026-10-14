#include "vulkan_video_image.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace lumaframevr {
VkSamplerYcbcrConversionCreateInfo video_sampling_description(VkFormat format, int colorspace, int range, const FrameColor& color, VkFormatFeatureFlags features) {
    constexpr std::array matrices{0, 1, 2, 5, 6, 9};
    constexpr std::array transfers{0, 1, 2, 4, 5, 6, 13, 14, 15, 16, 18};
    if(std::find(matrices.begin(), matrices.end(), colorspace) == matrices.end())
        throw std::invalid_argument("Unsupported YCbCr matrix; BT.2020 constant-luminance is not NCL");
    if(std::find(transfers.begin(), transfers.end(), color.transfer) == transfers.end())
        throw std::invalid_argument("Unsupported video transfer characteristic");
    if((color.transfer == 16 || color.transfer == 18) && color.primaries != 1 && color.primaries != 9)
        throw std::invalid_argument("HDR requires supported explicit BT.709/BT.2020 primaries");
    if(color.chroma_location < 0 || color.chroma_location > 4)
        throw std::invalid_argument("Unsupported chroma location");
    VkSamplerYcbcrConversionCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
    info.format = format;
    info.ycbcrModel = colorspace == 5 || colorspace == 6 ? VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601
                      : colorspace == 9                  ? VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020
                                                         : VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709;
    info.ycbcrRange = range >= 2 ? VK_SAMPLER_YCBCR_RANGE_ITU_FULL : VK_SAMPLER_YCBCR_RANGE_ITU_NARROW;
    info.xChromaOffset = color.chroma_location == 1 || color.chroma_location == 3 ? VK_CHROMA_LOCATION_COSITED_EVEN : VK_CHROMA_LOCATION_MIDPOINT;
    info.yChromaOffset = color.chroma_location == 3 || color.chroma_location == 4 ? VK_CHROMA_LOCATION_COSITED_EVEN : VK_CHROMA_LOCATION_MIDPOINT;
    info.chromaFilter = VK_FILTER_LINEAR;
    VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT;
    for(const auto location : {info.xChromaOffset, info.yChromaOffset})
        required |= location == VK_CHROMA_LOCATION_COSITED_EVEN ? VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT : VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT;
    if((features & required) != required)
        throw std::runtime_error("Unsupported YCbCr filtering/chroma location");
    return info;
}
void VulkanVideoImage::create_conversion(VkFormat format, VkFormatFeatureFlags features) {
    const auto info = video_sampling_description(format, frame_.colorspace(), frame_.color_range(), frame_.color(), features);
    if(!sampling_ || !sampling_->matches(device_, &info))
        sampling_ = std::make_shared<VulkanSampling>(device_, &info);
}
}  // namespace lumaframevr
