#include "openxr_passthrough.hpp"
#include <algorithm>
#include <stdexcept>
namespace lumaframevr {
PassthroughBackend select_passthrough(std::span<const XrEnvironmentBlendMode> modes, bool reconstruction) {
    if(reconstruction)
        return PassthroughBackend::reconstruction;
    if(std::find(modes.begin(), modes.end(), XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND) != modes.end())
        return PassthroughBackend::alpha;
    if(std::find(modes.begin(), modes.end(), XR_ENVIRONMENT_BLEND_MODE_ADDITIVE) != modes.end())
        return PassthroughBackend::additive;
    return PassthroughBackend::unavailable;
}
OpenXrPassthrough::OpenXrPassthrough(XrSession session, PassthroughBackend backend, PassthroughApi api) : session_(session), backend_(backend), api_(api) {
    layer_.type = XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB;
    if(backend == PassthroughBackend::reconstruction && (!api.create || !api.destroy || !api.start || !api.pause || !api.create_layer || !api.destroy_layer || !api.pause_layer || !api.resume_layer))
        throw std::invalid_argument("Incomplete reconstructed passthrough API");
    switch(backend_) {
        case PassthroughBackend::reconstruction:
            detail_ = "XR_FB_passthrough reconstructed camera underlay";
            break;
        case PassthroughBackend::alpha:
            detail_ = "Native OpenXR alpha-blended camera environment";
            break;
        case PassthroughBackend::additive:
            detail_ = "Native OpenXR additive camera environment; dark video pixels blend with surroundings";
            break;
        default:
            detail_ = "This runtime exposes neither camera passthrough nor a transparent environment blend mode";
    }
}
void OpenXrPassthrough::set_enabled(bool enabled) {
    if(enabled == enabled_)
        return;
    if(enabled && !available())
        throw std::runtime_error(detail_);
    if(backend_ == PassthroughBackend::reconstruction) {
        if(enabled) {
            if(!camera_) {
                XrPassthroughCreateInfoFB create{};
                create.type = XR_TYPE_PASSTHROUGH_CREATE_INFO_FB;
                xr_check(api_.create(session_, &create, &camera_), "create camera passthrough");
            }
            if(!layer_.layerHandle) {
                XrPassthroughLayerCreateInfoFB create{};
                create.type = XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB;
                create.passthrough = camera_;
                create.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
                xr_check(api_.create_layer(session_, &create, &layer_.layerHandle), "create reconstructed camera layer");
            }
            try {
                xr_check(api_.start(camera_), "start camera passthrough");
                xr_check(api_.resume_layer(layer_.layerHandle), "resume camera underlay");
            } catch(...) {
                api_.pause_layer(layer_.layerHandle);
                api_.pause(camera_);
                throw;
            }
        } else {
            const auto layer_result = api_.pause_layer(layer_.layerHandle), camera_result = api_.pause(camera_);
            enabled_ = false;
            xr_check(layer_result, "pause camera underlay");
            xr_check(camera_result, "pause camera passthrough");
        }
    }
    enabled_ = enabled;
}
XrEnvironmentBlendMode OpenXrPassthrough::blend_mode() const {
    if(enabled_ && backend_ == PassthroughBackend::alpha)
        return XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
    if(enabled_ && backend_ == PassthroughBackend::additive)
        return XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
    return XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
}
const XrCompositionLayerBaseHeader* OpenXrPassthrough::layer() const {
    return enabled_ && layer_.layerHandle ? reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer_) : nullptr;
}
OpenXrPassthrough::~OpenXrPassthrough() {
    if(enabled_ && backend_ == PassthroughBackend::reconstruction) {
        api_.pause_layer(layer_.layerHandle);
        api_.pause(camera_);
    }
    if(layer_.layerHandle)
        api_.destroy_layer(layer_.layerHandle);
    if(camera_)
        api_.destroy(camera_);
}
}  // namespace lumaframevr
