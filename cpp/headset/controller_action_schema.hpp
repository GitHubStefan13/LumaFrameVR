#pragma once
#include <openxr/openxr.h>
#include "controller_channels.hpp"
#include <array>
#include <string_view>
namespace lumaframevr {
struct ControllerActionDescription {
    std::string_view name;
    XrActionType type;
};
inline constexpr std::array<ControllerActionDescription, controller_button_count + controller_hand_count * controller_channel_count> controller_actions{{{"rotation", XR_ACTION_TYPE_BOOLEAN_INPUT},
                                                                                                                                                         {"confirm", XR_ACTION_TYPE_BOOLEAN_INPUT},
                                                                                                                                                         {"back", XR_ACTION_TYPE_BOOLEAN_INPUT},
                                                                                                                                                         {"settings", XR_ACTION_TYPE_BOOLEAN_INPUT},
                                                                                                                                                         {"up", XR_ACTION_TYPE_BOOLEAN_INPUT},
                                                                                                                                                         {"down", XR_ACTION_TYPE_BOOLEAN_INPUT},
                                                                                                                                                         {"left", XR_ACTION_TYPE_BOOLEAN_INPUT},
                                                                                                                                                         {"right", XR_ACTION_TYPE_BOOLEAN_INPUT},
                                                                                                                                                         {"left_grip", XR_ACTION_TYPE_FLOAT_INPUT},
                                                                                                                                                         {"left_trigger", XR_ACTION_TYPE_FLOAT_INPUT},
                                                                                                                                                         {"left_stick", XR_ACTION_TYPE_VECTOR2F_INPUT},
                                                                                                                                                         {"left_pose", XR_ACTION_TYPE_POSE_INPUT},
                                                                                                                                                         {"left_aim", XR_ACTION_TYPE_POSE_INPUT},
                                                                                                                                                         {"right_grip", XR_ACTION_TYPE_FLOAT_INPUT},
                                                                                                                                                         {"right_trigger", XR_ACTION_TYPE_FLOAT_INPUT},
                                                                                                                                                         {"right_stick", XR_ACTION_TYPE_VECTOR2F_INPUT},
                                                                                                                                                         {"right_pose", XR_ACTION_TYPE_POSE_INPUT},
                                                                                                                                                         {"right_aim", XR_ACTION_TYPE_POSE_INPUT}}};
inline constexpr std::array<const char*, 4> controller_profiles{"/interaction_profiles/valve/frame_controller_valve", "/interaction_profiles/oculus/touch_controller",
                                                                "/interaction_profiles/valve/index_controller", "/interaction_profiles/khr/simple_controller"};
}  // namespace lumaframevr
