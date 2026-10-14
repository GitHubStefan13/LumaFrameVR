#include "openxr_session.hpp"
#include "openxr_composition.hpp"
#include <stdexcept>
namespace lumaframevr {
OpenXrFrame::OpenXrFrame(OpenXrSession& session) : session_(session) {
    state_.type = XR_TYPE_FRAME_STATE;
    XrFrameWaitInfo wait{};
    wait.type = XR_TYPE_FRAME_WAIT_INFO;
    xr_check(xrWaitFrame(session_.handle(), &wait, &state_), "wait XR frame");
    XrFrameBeginInfo begin{};
    begin.type = XR_TYPE_FRAME_BEGIN_INFO;
    xr_check(xrBeginFrame(session_.handle(), &begin), "begin XR frame");
    active_ = true;
    try {
        for(auto& view : views_) view.type = XR_TYPE_VIEW;
        XrViewLocateInfo locate{};
        locate.type = XR_TYPE_VIEW_LOCATE_INFO;
        locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locate.displayTime = state_.predictedDisplayTime;
        locate.space = session_.local_space();
        view_state_.type = XR_TYPE_VIEW_STATE;
        std::uint32_t count{};
        xr_check(xrLocateViews(session_.handle(), &locate, &view_state_, 2, &count, views_.data()), "locate stereo views");
        if(count != 2)
            throw std::runtime_error("XR returned invalid stereo view count");
    } catch(...) {
        finish({}, false);
        throw;
    }
}
bool OpenXrFrame::renderable() const {
    auto valid = XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
    return state_.shouldRender && (view_state_.viewStateFlags & valid) == valid;
}
VkImage OpenXrFrame::acquire() {
    if((image_ && image_->active()) || !active_ || !renderable())
        throw std::logic_error("Invalid XR swapchain acquire");
    image_.emplace(session_.swapchain(), "acquire stereo swapchain image");
    image_->wait("wait stereo swapchain image");
    return session_.images().at(image_->index()).image;
}
void OpenXrFrame::finish(std::span<const XrCompositionLayerBaseHeader* const> panels, bool project, XrEnvironmentBlendMode blend, const XrCompositionLayerBaseHeader* underlay) {
    if(!active_)
        return;
    if(image_ && image_->active()) {
        image_->release("release stereo swapchain image", ReleaseFailure::retain);
        rendered_ = true;
    }
    std::array<XrCompositionLayerProjectionView, 2> projection_views{};
    for(std::size_t eye = 0; eye < 2; ++eye) {
        auto& view = projection_views[eye];
        view.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        view.pose = views_[eye].pose;
        view.fov = views_[eye].fov;
        view.subImage.swapchain = session_.swapchain();
        view.subImage.imageArrayIndex = static_cast<std::uint32_t>(eye);
        view.subImage.imageRect.extent = {static_cast<std::int32_t>(session_.extent().width), static_cast<std::int32_t>(session_.extent().height)};
    }
    XrCompositionLayerProjection projection{};
    projection.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
    projection.space = session_.local_space();
    projection.viewCount = 2;
    projection.views = projection_views.data();
    const auto layers = frame_layers(projection, rendered_ && project, panels, underlay, underlay || blend != XR_ENVIRONMENT_BLEND_MODE_OPAQUE);
    XrFrameEndInfo end{};
    end.type = XR_TYPE_FRAME_END_INFO;
    end.displayTime = state_.predictedDisplayTime;
    end.environmentBlendMode = blend;
    end.layerCount = static_cast<std::uint32_t>(layers.size());
    end.layers = layers.data();
    auto result = xrEndFrame(session_.handle(), &end);
    active_ = false;
    xr_check(result, "end XR frame");
}
OpenXrFrame::~OpenXrFrame() {
    if(active_)
        try {
            finish({}, false);
        } catch(...) {
            if(image_)
                image_->abandon();
        }
}
}  // namespace lumaframevr
