#pragma once
#include "openxr_session.hpp"
namespace lumaframevr {
enum class PassthroughBackend { unavailable, alpha, additive, reconstruction };
PassthroughBackend select_passthrough(std::span<const XrEnvironmentBlendMode> modes, bool reconstruction);
struct PassthroughApi {
    PFN_xrCreatePassthroughFB create{};
    PFN_xrDestroyPassthroughFB destroy{};
    PFN_xrPassthroughStartFB start{};
    PFN_xrPassthroughPauseFB pause{};
    PFN_xrCreatePassthroughLayerFB create_layer{};
    PFN_xrDestroyPassthroughLayerFB destroy_layer{};
    PFN_xrPassthroughLayerPauseFB pause_layer{};
    PFN_xrPassthroughLayerResumeFB resume_layer{};
};
class OpenXrPassthrough {
   public:
    explicit OpenXrPassthrough(OpenXrSession& session);
    OpenXrPassthrough(XrSession session, PassthroughBackend backend, PassthroughApi api);
    ~OpenXrPassthrough();
    OpenXrPassthrough(const OpenXrPassthrough&) = delete;
    OpenXrPassthrough& operator=(const OpenXrPassthrough&) = delete;
    bool available() const { return backend_ != PassthroughBackend::unavailable; }
    bool enabled() const { return enabled_; }
    const std::string& detail() const { return detail_; }
    XrEnvironmentBlendMode blend_mode() const;
    const XrCompositionLayerBaseHeader* layer() const;
    void set_enabled(bool enabled);

   private:
    XrSession session_{};
    PassthroughBackend backend_{PassthroughBackend::unavailable};
    PassthroughApi api_;
    XrPassthroughFB camera_{};
    XrCompositionLayerPassthroughFB layer_{};
    bool enabled_{};
    std::string detail_;
};
}  // namespace lumaframevr
