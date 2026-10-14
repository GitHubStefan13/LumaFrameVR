#pragma once
#include "openxr_session.hpp"
#include "controller_pose_filter.hpp"
#include "controller_state.hpp"
#include "controller_action_schema.hpp"
#include <array>
#include <optional>
namespace lumaframevr {
struct ControllerBinding {
    unsigned action;
    std::string path;
};
std::vector<ControllerBinding> controller_bindings(std::string_view profile);
class OpenXrInput {
   public:
    explicit OpenXrInput(OpenXrSession& session);
    ~OpenXrInput();
    OpenXrInput(const OpenXrInput&) = delete;
    OpenXrInput& operator=(const OpenXrInput&) = delete;
    ControllerState poll(XrTime time);

   private:
    XrPath path(const std::string& text) const;
    std::optional<Pose> locate(XrAction action, XrSpace space, XrTime time) const;
    void reset() noexcept;
    OpenXrSession& session_;
    XrActionSet set_{};
    std::array<XrAction, controller_actions.size()> actions_{};
    std::array<XrSpace, 2> grip_spaces_{}, aim_spaces_{};
    std::array<XrPath, 2> users_{}, profiles_{};
    std::array<ControllerPoseFilter, 2> grip_filters_, aim_filters_;
};
}  // namespace lumaframevr
