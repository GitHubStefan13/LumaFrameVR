#pragma once
namespace lumaframevr {
enum class ControllerButton : unsigned { rotation, confirm, back, settings, up, down, left, right, count };
enum class ControllerChannel : unsigned { grip, trigger, stick, pose, aim, count };
inline constexpr unsigned controller_button_count = static_cast<unsigned>(ControllerButton::count);
inline constexpr unsigned controller_channel_count = static_cast<unsigned>(ControllerChannel::count);
inline constexpr unsigned controller_hand_count = 2;
constexpr unsigned action_index(ControllerButton button) {
    return static_cast<unsigned>(button);
}
constexpr unsigned action_index(unsigned hand, ControllerChannel channel) {
    return controller_button_count + hand * controller_channel_count + static_cast<unsigned>(channel);
}
}  // namespace lumaframevr
