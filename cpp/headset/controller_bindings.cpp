#include "openxr_input.hpp"
#include <stdexcept>
namespace lumaframevr {
std::vector<ControllerBinding> controller_bindings(std::string_view profile) {
    bool frame = profile == "/interaction_profiles/valve/frame_controller_valve";
    bool touch = profile == "/interaction_profiles/oculus/touch_controller";
    bool index = profile == "/interaction_profiles/valve/index_controller";
    bool simple = profile == "/interaction_profiles/khr/simple_controller";
    if(!frame && !touch && !index && !simple)
        throw std::invalid_argument("Unknown controller profile");
    std::vector<ControllerBinding> result;
    auto bind = [&](unsigned action, std::string_view hand, std::string_view field) { result.push_back({action, "/user/hand/" + std::string(hand) + "/input/" + std::string(field)}); };
    for(unsigned hand = 0; hand < controller_hand_count; ++hand) {
        std::string_view name = hand ? "right" : "left";
        bind(action_index(hand, ControllerChannel::pose), name, "grip/pose");
        bind(action_index(hand, ControllerChannel::aim), name, "aim/pose");
        if(simple)
            continue;
        bind(action_index(hand, ControllerChannel::grip), name, "squeeze/value");
        bind(action_index(hand, ControllerChannel::trigger), name, "trigger/value");
        bind(action_index(hand, ControllerChannel::stick), name, "thumbstick");
        if(frame) {
            bind(action_index(ControllerButton::left), name, "shoulder/left/click");
            bind(action_index(ControllerButton::right), name, "shoulder/right/click");
        }
    }
    if(simple) {
        bind(action_index(ControllerButton::confirm), "right", "select/click");
        bind(action_index(ControllerButton::back), "left", "select/click");
        return result;
    }
    bind(action_index(ControllerButton::confirm), "right", "a/click");
    bind(action_index(ControllerButton::back), "right", "b/click");
    bind(action_index(ControllerButton::rotation), frame ? "right" : "left", index ? "a/click" : "x/click");
    bind(action_index(ControllerButton::settings), frame ? "right" : "left", index ? "b/click" : "y/click");
    if(frame) {
        bind(action_index(ControllerButton::up), "left", "dpad_up/click");
        bind(action_index(ControllerButton::down), "left", "dpad_down/click");
        bind(action_index(ControllerButton::left), "left", "dpad_left/click");
        bind(action_index(ControllerButton::right), "left", "dpad_right/click");
    }
    return result;
}
}  // namespace lumaframevr
