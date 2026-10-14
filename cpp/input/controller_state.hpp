#pragma once
#include "controller_channels.hpp"
#include "pose_math.hpp"
#include <array>
#include <optional>
namespace lumaframevr {
struct ControllerState {
    std::array<bool, controller_button_count> pressed{}, held{};
    std::array<bool, 2> grips{};
    std::array<float, 2> triggers{};
    std::array<std::array<float, 2>, 2> sticks{};
    std::array<std::optional<Pose>, 2> grip_poses, aim_poses;
};
}  // namespace lumaframevr
