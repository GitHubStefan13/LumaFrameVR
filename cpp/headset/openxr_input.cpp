#include "openxr_input.hpp"
#include <cstdio>
#include <cstring>
#include <iostream>
namespace lumaframevr {
XrPath OpenXrInput::path(const std::string& text) const {
    XrPath result{};
    xr_check(xrStringToPath(session_.runtime()->handle(), text.c_str(), &result), "resolve controller path");
    return result;
}
OpenXrInput::OpenXrInput(OpenXrSession& session) : session_(session) {
    try {
        XrActionSetCreateInfo set{};
        set.type = XR_TYPE_ACTION_SET_CREATE_INFO;
        std::snprintf(set.actionSetName, sizeof(set.actionSetName), "%s", "player");
        std::snprintf(set.localizedActionSetName, sizeof(set.localizedActionSetName), "%s", "LumaFrameVR player");
        xr_check(xrCreateActionSet(session.runtime()->handle(), &set, &set_), "create player action set");
        for(unsigned index = 0; index < controller_actions.size(); ++index) {
            const auto& description = controller_actions[index];
            XrActionCreateInfo info{};
            info.type = XR_TYPE_ACTION_CREATE_INFO;
            info.actionType = description.type;
            std::strncpy(info.actionName, description.name.data(), XR_MAX_ACTION_NAME_SIZE - 1);
            std::strncpy(info.localizedActionName, description.name.data(), XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
            xr_check(xrCreateAction(set_, &info, &actions_[index]), "create controller action");
        }
        for(const char* profile : controller_profiles) {
            std::vector<XrActionSuggestedBinding> bindings;
            for(const auto& binding : controller_bindings(profile)) bindings.push_back({actions_[binding.action], path(binding.path)});
            XrInteractionProfileSuggestedBinding suggestion{};
            suggestion.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
            suggestion.interactionProfile = path(profile);
            suggestion.countSuggestedBindings = static_cast<std::uint32_t>(bindings.size());
            suggestion.suggestedBindings = bindings.data();
            auto result = xrSuggestInteractionProfileBindings(session.runtime()->handle(), &suggestion);
            if(XR_FAILED(result))
                std::cerr << "Controller bindings refused: " << profile << " OpenXR=" << result << '\n';
        }
        for(unsigned hand = 0; hand < controller_hand_count; ++hand) {
            users_[hand] = path(hand ? "/user/hand/right" : "/user/hand/left");
            XrActionSpaceCreateInfo space{};
            space.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
            space.poseInActionSpace.orientation.w = 1;
            space.action = actions_[action_index(hand, ControllerChannel::pose)];
            xr_check(xrCreateActionSpace(session.handle(), &space, &grip_spaces_[hand]), "create grip action space");
            space.action = actions_[action_index(hand, ControllerChannel::aim)];
            xr_check(xrCreateActionSpace(session.handle(), &space, &aim_spaces_[hand]), "create aim action space");
        }
        XrSessionActionSetsAttachInfo attach{};
        attach.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
        attach.countActionSets = 1;
        attach.actionSets = &set_;
        xr_check(xrAttachSessionActionSets(session.handle(), &attach), "attach player actions");
    } catch(...) {
        reset();
        throw;
    }
}
void OpenXrInput::reset() noexcept {
    for(auto space : grip_spaces_)
        if(space)
            xrDestroySpace(space);
    for(auto space : aim_spaces_)
        if(space)
            xrDestroySpace(space);
    if(set_)
        xrDestroyActionSet(set_);
}
OpenXrInput::~OpenXrInput() {
    reset();
}
}  // namespace lumaframevr
