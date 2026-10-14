#pragma once
#include "openxr_runtime.hpp"
#include "openxr_swapchain.hpp"
#include <optional>
#include <vector>
#include <span>
namespace lumaframevr {
class OpenXrFrame;
class OpenXrSession {
   public:
    explicit OpenXrSession(std::shared_ptr<OpenXrRuntime> runtime);
    ~OpenXrSession();
    OpenXrSession(const OpenXrSession&) = delete;
    OpenXrSession& operator=(const OpenXrSession&) = delete;
    bool poll();
    bool running() const { return running_; }
    // The runtime asked the session to end (EXITING or LOSS_PENDING). A player with no
    // session menu of its own is done at that point: keep going and the runtime has to
    // force-kill the process, which is what a broken Steam Exit Game looks like.
    bool exiting() const { return exiting_; }
    // The instance is being lost, so nothing may be destroyed through the runtime any more:
    // those calls land in a runtime that is tearing down and null-dereference. Only that case
    // abandons. A runtime that merely asked us to exit is still alive and expects the ordinary
    // teardown, which is what tells SteamVR the application really is gone.
    bool instance_lost() const { return instance_lost_; }
    void abandon() { abandoned_ = true; }
    // The viewer asked to quit: ask the runtime to end the session instead of tearing it down
    // behind its back, so it runs the same bookkeeping as the overlay's Exit Game.
    void request_exit();
    // End the session before destroying it when the frame loop is leaving while it is still
    // running, which is what the specification asks for.
    void end_session();
    bool exit_requested() const { return exit_requested_; }
    bool focused() const { return state_ == XR_SESSION_STATE_FOCUSED; }
    bool passthrough_fault() const { return passthrough_fault_; }
    XrSession handle() const { return session_; }
    XrSpace local_space() const { return local_; }
    XrSpace view_space() const { return view_; }
    XrSwapchain swapchain() const { return swapchain_; }
    VkFormat format() const { return format_; }
    VkExtent2D extent() const { return extent_; }
    const std::vector<XrSwapchainImageVulkan2KHR>& images() const { return images_; }
    std::shared_ptr<OpenXrRuntime> runtime() const { return runtime_; }
    std::shared_ptr<VulkanDevice> graphics() const { return graphics_; }
    std::unique_ptr<OpenXrFrame> begin_frame();

   private:
    void reset() noexcept;
    std::shared_ptr<OpenXrRuntime> runtime_;
    std::shared_ptr<VulkanDevice> graphics_;
    XrSession session_{};
    XrSpace local_{}, view_{};
    XrSwapchain swapchain_{};
    VkFormat format_{};
    VkExtent2D extent_{};
    std::vector<XrSwapchainImageVulkan2KHR> images_;
    XrSessionState state_{XR_SESSION_STATE_UNKNOWN};
    bool running_{}, exiting_{}, passthrough_fault_{}, abandoned_{}, instance_lost_{}, exit_requested_{};
};
// How to leave the session when the frame loop is over. Ending it is safe and is what the
// runtime asks for at STOPPING. Destroying it is not: on this runtime xrDestroySession lands
// in vrclient.so and takes the process down, with the frame stack showing
// vrclient.so -> VkLayer_VALVE_fdm_injection -> libopenxr_loader. Leaving the session to the
// runtime and ending the process is what the specification asks for when the application has
// no non-XR experience. LUMAFRAMEVR_DESTROY_XR_SESSION_ON_EXIT=1 attempts the destroying
// teardown instead, for a runtime that handles it.
void leave_session(OpenXrSession& session);
class OpenXrFrame {
   public:
    explicit OpenXrFrame(OpenXrSession& session);
    ~OpenXrFrame();
    OpenXrFrame(const OpenXrFrame&) = delete;
    OpenXrFrame& operator=(const OpenXrFrame&) = delete;
    XrTime display_time() const { return state_.predictedDisplayTime; }
    bool renderable() const;
    const std::array<XrView, 2>& views() const { return views_; }
    VkImage acquire();
    void finish(std::span<const XrCompositionLayerBaseHeader* const> panels = {}, bool project = true, XrEnvironmentBlendMode blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
                const XrCompositionLayerBaseHeader* underlay = nullptr);

   private:
    OpenXrSession& session_;
    XrFrameState state_{};
    XrViewState view_state_{};
    std::array<XrView, 2> views_{};
    bool active_{}, rendered_{};
    std::optional<AcquiredSwapchainImage> image_;
};
}  // namespace lumaframevr
