#pragma once
#include "openxr_session.hpp"
#include "canvas.hpp"
namespace lumaframevr {
class OpenXrPanel {
   public:
    OpenXrPanel(OpenXrSession& session, int width, int height);
    ~OpenXrPanel();
    OpenXrPanel(const OpenXrPanel&) = delete;
    OpenXrPanel& operator=(const OpenXrPanel&) = delete;
    void upload(const ui::Canvas& canvas);
    XrCompositionLayerQuad layer(XrSpace space, XrPosef pose, float reference_width) const;

   private:
    OpenXrSession& session_;
    XrSwapchain chain_{};
    std::vector<XrSwapchainImageVulkan2KHR> images_;
    int width_, height_;
};
}  // namespace lumaframevr
