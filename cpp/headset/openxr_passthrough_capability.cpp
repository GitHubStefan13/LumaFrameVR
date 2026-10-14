#include "openxr_passthrough.hpp"
#include <stdexcept>
namespace lumaframevr {
OpenXrPassthrough::OpenXrPassthrough(OpenXrSession& session) : OpenXrPassthrough(session.handle(), PassthroughBackend::unavailable, {}) {
    try {
        const auto runtime = session.runtime();
        std::uint32_t count{};
        xr_check(xrEnumerateEnvironmentBlendModes(runtime->handle(), runtime->system(), XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &count, nullptr), "enumerate passthrough blend modes");
        if(!count || count > 32)
            throw std::runtime_error("Invalid runtime environment blend-mode list");
        std::vector<XrEnvironmentBlendMode> modes(count);
        xr_check(xrEnumerateEnvironmentBlendModes(runtime->handle(), runtime->system(), XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, count, &count, modes.data()), "read passthrough blend modes");
        bool reconstruction{};
        if(runtime->passthrough_extension()) {
            XrSystemPassthroughPropertiesFB camera{};
            camera.type = XR_TYPE_SYSTEM_PASSTHROUGH_PROPERTIES_FB;
            XrSystemProperties system{};
            system.type = XR_TYPE_SYSTEM_PROPERTIES;
            system.next = &camera;
            xr_check(xrGetSystemProperties(runtime->handle(), runtime->system(), &system), "query camera passthrough support");
            reconstruction = camera.supportsPassthrough;
        }
        backend_ = select_passthrough(modes, reconstruction);
        if(reconstruction) {
            api_ = {runtime->function<PFN_xrCreatePassthroughFB>("xrCreatePassthroughFB"),
                    runtime->function<PFN_xrDestroyPassthroughFB>("xrDestroyPassthroughFB"),
                    runtime->function<PFN_xrPassthroughStartFB>("xrPassthroughStartFB"),
                    runtime->function<PFN_xrPassthroughPauseFB>("xrPassthroughPauseFB"),
                    runtime->function<PFN_xrCreatePassthroughLayerFB>("xrCreatePassthroughLayerFB"),
                    runtime->function<PFN_xrDestroyPassthroughLayerFB>("xrDestroyPassthroughLayerFB"),
                    runtime->function<PFN_xrPassthroughLayerPauseFB>("xrPassthroughLayerPauseFB"),
                    runtime->function<PFN_xrPassthroughLayerResumeFB>("xrPassthroughLayerResumeFB")};
        }
        OpenXrPassthrough description(session.handle(), backend_, api_);
        detail_ = description.detail();
        detail_ += "; advertised blend modes=";
        for(auto mode : modes) detail_ += std::to_string(static_cast<int>(mode)) + " ";
    } catch(const std::exception& error) {
        backend_ = PassthroughBackend::unavailable;
        detail_ = "Camera passthrough unavailable: " + std::string(error.what());
    }
}
}  // namespace lumaframevr
