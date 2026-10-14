#include "test_registry.hpp"
// Controller binding acceptance.
//
// This test asserts behaviour, not the literal button paths. Remapping a face
// button (for example moving confirm from A to B) must not require editing this
// file: it checks that every profile binds the actions the application needs and
// that each binding's path suffix matches the action's declared type. The exact
// physical button behind an action is a product decision, not a contract.
#include "controller_action_schema.hpp"
#include "openxr_input.hpp"
#include "test_assertions.hpp"
#include <algorithm>
#include <set>
#include <string>

using namespace lumaframevr;

namespace {
// The suffix of a binding path determines the OpenXR input type it must carry.
XrActionType type_for_path(std::string_view path) {
    if(path.ends_with("/pose"))
        return XR_ACTION_TYPE_POSE_INPUT;
    if(path.ends_with("/value"))
        return XR_ACTION_TYPE_FLOAT_INPUT;
    if(path.ends_with("/thumbstick"))
        return XR_ACTION_TYPE_VECTOR2F_INPUT;
    return XR_ACTION_TYPE_BOOLEAN_INPUT;
}

bool binds(const std::vector<ControllerBinding>& bindings, unsigned action) {
    return std::any_of(bindings.begin(), bindings.end(), [action](const ControllerBinding& binding) { return binding.action == action; });
}
}  // namespace

int lumaframevr::tests::controller_bindings(int, char**) {
    return acceptance_main([] {
        // The action schema is a stable contract: names, types and indices.
        require(controller_actions.size() == 18 && controller_button_count == 8, "action schema dimensions");
        require(action_index(ControllerButton::rotation) == 0 && action_index(ControllerButton::confirm) == 1 && action_index(ControllerButton::back) == 2 &&
                        action_index(ControllerButton::settings) == 3,
                "named face slots");
        require(action_index(ControllerButton::up) == 4 && action_index(ControllerButton::down) == 5 && action_index(ControllerButton::left) == 6 && action_index(ControllerButton::right) == 7,
                "named direction slots");
        for(unsigned hand = 0; hand < controller_hand_count; ++hand) {
            require(action_index(hand, ControllerChannel::grip) == 8 + hand * 5, "grip index");
            require(action_index(hand, ControllerChannel::trigger) == 9 + hand * 5, "trigger index");
            require(action_index(hand, ControllerChannel::stick) == 10 + hand * 5, "stick index");
            require(action_index(hand, ControllerChannel::pose) == 11 + hand * 5, "pose index");
            require(action_index(hand, ControllerChannel::aim) == 12 + hand * 5, "aim index");
        }

        for(const char* profile : controller_profiles) {
            const bool simple = std::string_view(profile) == "/interaction_profiles/khr/simple_controller";
            const auto bindings = lumaframevr::controller_bindings(profile);
            require(!bindings.empty(), "profile produces bindings");

            // Every binding targets a real action and a type-consistent path.
            std::set<std::pair<unsigned, std::string>> unique;
            for(const auto& binding : bindings) {
                require(binding.action < controller_actions.size(), "binding within action array");
                require(binding.path.starts_with("/user/hand/"), "binding is a hand input path");
                require(controller_actions[binding.action].type == type_for_path(binding.path), "binding path suffix matches the action type");
                require(unique.insert({binding.action, binding.path}).second, "no duplicate binding");
            }

            // Actions the application always needs, on every profile.
            require(binds(bindings, action_index(ControllerButton::confirm)), "confirm is bound");
            require(binds(bindings, action_index(ControllerButton::back)), "back is bound");
            for(unsigned hand = 0; hand < controller_hand_count; ++hand) {
                require(binds(bindings, action_index(hand, ControllerChannel::pose)), "hand pose is bound");
                require(binds(bindings, action_index(hand, ControllerChannel::aim)), "hand aim is bound");
            }

            // Full profiles additionally need the analog channels and the
            // rotation/settings face actions; the simple profile does not.
            if(!simple) {
                require(binds(bindings, action_index(ControllerButton::rotation)), "rotation is bound");
                require(binds(bindings, action_index(ControllerButton::settings)), "settings is bound");
                for(unsigned hand = 0; hand < controller_hand_count; ++hand) {
                    require(binds(bindings, action_index(hand, ControllerChannel::grip)), "grip is bound");
                    require(binds(bindings, action_index(hand, ControllerChannel::trigger)), "trigger is bound");
                    require(binds(bindings, action_index(hand, ControllerChannel::stick)), "stick is bound");
                }
            }
        }

        rejects([] { lumaframevr::controller_bindings("unknown"); }, "unknown profiles fail closed");
        std::cout << "controller bindings: every profile binds the required actions with type-consistent paths\n";
    });
}
