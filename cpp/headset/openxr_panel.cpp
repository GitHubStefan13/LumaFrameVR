#include "openxr_panel.hpp"
#include "openxr_swapchain.hpp"
#include "panel_image_upload.hpp"
#include <algorithm>
#include <stdexcept>
namespace lumaframevr {
OpenXrPanel::OpenXrPanel(OpenXrSession& session, int width, int height) : session_(session), width_(width), height_(height) {
    if(width <= 0 || height <= 0)
        throw std::invalid_argument("Invalid XR panel dimensions");
    std::uint32_t count{};
    xr_check(xrEnumerateSwapchainFormats(session.handle(), 0, &count, nullptr), "enumerate panel formats");
    std::vector<std::int64_t> formats(count);
    xr_check(xrEnumerateSwapchainFormats(session.handle(), count, &count, formats.data()), "enumerate panel formats");
    if(std::find(formats.begin(), formats.end(), VK_FORMAT_R8G8B8A8_SRGB) == formats.end())
        throw std::runtime_error("XR panel requires sRGB RGBA swapchain");
    try {
        XrSwapchainCreateInfo info{};
        info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        info.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        info.format = VK_FORMAT_R8G8B8A8_SRGB;
        info.sampleCount = 1;
        info.width = width;
        info.height = height;
        info.faceCount = 1;
        info.arraySize = 1;
        info.mipCount = 1;
        xr_check(xrCreateSwapchain(session.handle(), &info, &chain_), "create UI panel swapchain");
        images_ = swapchain_images(chain_, "enumerate UI panel images");
    } catch(...) {
        if(chain_)
            xrDestroySwapchain(chain_);
        throw;
    }
}
void OpenXrPanel::upload(const ui::Canvas& canvas) {
    if(canvas.width() != width_ || canvas.height() != height_)
        throw std::invalid_argument("XR panel canvas dimensions changed");
    AcquiredSwapchainImage image(chain_, "acquire UI panel image");
    image.wait("wait UI panel image");
    upload_panel_image(session_.graphics(), images_.at(image.index()).image, canvas);
    image.release("release UI panel image");
}
XrCompositionLayerQuad OpenXrPanel::layer(XrSpace space, XrPosef pose, float reference_width) const {
    XrCompositionLayerQuad result{};
    result.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    result.space = space;
    result.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    result.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    result.pose = pose;
    result.size = {reference_width * width_ / 792.0F, reference_width * height_ / 792.0F};
    result.subImage.swapchain = chain_;
    result.subImage.imageRect.extent = {width_, height_};
    return result;
}
OpenXrPanel::~OpenXrPanel() {
    if(chain_)
        xrDestroySwapchain(chain_);
}
}  // namespace lumaframevr
