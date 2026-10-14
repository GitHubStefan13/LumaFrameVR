#include "openxr_input.hpp"
#include <iostream>
namespace lumaframevr {
std::optional<Pose> OpenXrInput::locate(XrAction action, XrSpace space, XrTime time) const {
    XrActionStateGetInfo get{};
    get.type = XR_TYPE_ACTION_STATE_GET_INFO;
    get.action = action;
    XrActionStatePose state{};
    state.type = XR_TYPE_ACTION_STATE_POSE;
    xr_check(xrGetActionStatePose(session_.handle(), &get, &state), "read controller pose activity");
    if(!state.isActive)
        return std::nullopt;
    XrSpaceLocation location{};
    location.type = XR_TYPE_SPACE_LOCATION;
    xr_check(xrLocateSpace(space, session_.local_space(), time, &location), "locate controller pose");
    auto valid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
    if((location.locationFlags & valid) != valid)
        return std::nullopt;
    return Pose{{location.pose.position.x, location.pose.position.y, location.pose.position.z},
                {location.pose.orientation.x, location.pose.orientation.y, location.pose.orientation.z, location.pose.orientation.w}};
}
ControllerState OpenXrInput::poll(XrTime time) {
    ControllerState result;
    XrActiveActionSet active{set_, XR_NULL_PATH};
    XrActionsSyncInfo sync{};
    sync.type = XR_TYPE_ACTIONS_SYNC_INFO;
    sync.countActiveActionSets = 1;
    sync.activeActionSets = &active;
    auto status = xrSyncActions(session_.handle(), &sync);
    if(status == XR_SESSION_NOT_FOCUSED)
        return result;
    xr_check(status, "synchronize controller actions");
    for(unsigned index = 0; index < controller_button_count; ++index) {
        XrActionStateGetInfo get{};
        get.type = XR_TYPE_ACTION_STATE_GET_INFO;
        get.action = actions_[index];
        XrActionStateBoolean state{};
        state.type = XR_TYPE_ACTION_STATE_BOOLEAN;
        xr_check(xrGetActionStateBoolean(session_.handle(), &get, &state), "read controller buttons");
        result.held[index] = state.isActive && state.currentState;
        result.pressed[index] = result.held[index] && state.changedSinceLastSync;
    }
    for(unsigned hand = 0; hand < controller_hand_count; ++hand) {
        result.grip_poses[hand] = grip_filters_[hand].update(locate(actions_[action_index(hand, ControllerChannel::pose)], grip_spaces_[hand], time), time);
        result.aim_poses[hand] = aim_filters_[hand].update(locate(actions_[action_index(hand, ControllerChannel::aim)], aim_spaces_[hand], time), time);
        XrActionStateGetInfo get{};
        get.type = XR_TYPE_ACTION_STATE_GET_INFO;
        get.action = actions_[action_index(hand, ControllerChannel::grip)];
        XrActionStateFloat grip{};
        grip.type = XR_TYPE_ACTION_STATE_FLOAT;
        xr_check(xrGetActionStateFloat(session_.handle(), &get, &grip), "read controller grip");
        result.grips[hand] = grip.isActive && grip.currentState > 0 && result.grip_poses[hand].has_value();
        get.action = actions_[action_index(hand, ControllerChannel::trigger)];
        XrActionStateFloat trigger{};
        trigger.type = XR_TYPE_ACTION_STATE_FLOAT;
        xr_check(xrGetActionStateFloat(session_.handle(), &get, &trigger), "read controller trigger");
        if(trigger.isActive)
            result.triggers[hand] = trigger.currentState;
        get.action = actions_[action_index(hand, ControllerChannel::stick)];
        XrActionStateVector2f stick{};
        stick.type = XR_TYPE_ACTION_STATE_VECTOR2F;
        xr_check(xrGetActionStateVector2f(session_.handle(), &get, &stick), "read controller stick");
        if(stick.isActive)
            result.sticks[hand] = {stick.currentState.x, stick.currentState.y};
        XrInteractionProfileState profile{};
        profile.type = XR_TYPE_INTERACTION_PROFILE_STATE;
        xr_check(xrGetCurrentInteractionProfile(session_.handle(), users_[hand], &profile), "read current controller profile");
        if(profile.interactionProfile != profiles_[hand]) {
            profiles_[hand] = profile.interactionProfile;
            std::array<char, 256> name{};
            std::uint32_t length{};
            if(profile.interactionProfile)
                xr_check(xrPathToString(session_.runtime()->handle(), profile.interactionProfile, name.size(), &length, name.data()), "resolve current controller profile");
            std::cerr << "Input " << (hand ? "right" : "left") << " profile=" << (profile.interactionProfile ? name.data() : "none") << '\n';
        }
    }
    return result;
}
}  // namespace lumaframevr
