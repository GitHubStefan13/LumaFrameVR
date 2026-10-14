#include "model_openvr.hpp"
#include <stdexcept>
namespace lumaframevr::application::model_detail {
namespace {
template <class F>
std::string component_text(F read, bool optional = false) {
    auto size = read(nullptr, 0);
    if(!size && optional)
        return {};
    if(!size || size > 4096)
        throw std::runtime_error("invalid OpenVR component name size");
    std::string result(size, '\0');
    if(read(result.data(), size) != size || result.back() != '\0')
        throw std::runtime_error("OpenVR component name changed or unterminated");
    result.pop_back();
    return result;
}
}  // namespace
void discover_parts(Session& session, HandWork& hand) {
    auto* api = session.models;
    VRControllerState_t neutral{};
    RenderModel_ControllerMode_State_t mode{};
    RenderModel_ComponentState_t state{};
    std::string grip = "openxr_grip";
    if(api->GetComponentState(hand.name.data(), grip.data(), &neutral, &mode, &state))
        hand.grip_from_model = inverse_rigid(matrix_from(state.mTrackingToComponentLocal));
    auto count = api->GetComponentCount(hand.name.data());
    if(count > 512)
        throw std::runtime_error("excessive OpenVR controller components");
    if(!count) {
        PartWork part;
        part.name = hand.name;
        part.model_from_part = identity();
        hand.parts.push_back(std::move(part));
        return;
    }
    for(uint32_t i = 0; i < count; ++i) {
        auto component = component_text([&](char* output, uint32_t capacity) { return api->GetComponentName(hand.name.data(), i, output, capacity); });
        auto name = component_text([&](char* output, uint32_t capacity) { return api->GetComponentRenderModelName(hand.name.data(), component.data(), output, capacity); }, true);
        if(name.empty())
            continue;
        state = {};
        if(!api->GetComponentState(hand.name.data(), component.data(), &neutral, &mode, &state))
            throw std::runtime_error("missing OpenVR component transform: " + component);
        PartWork part;
        part.name = std::move(name);
        part.component = std::move(component);
        part.model_from_part = matrix_from(state.mTrackingToComponentRenderModel);
        part.visible = (state.uProperties & EVRComponentProperty_VRComponentProperty_IsVisible) != 0;
        hand.parts.push_back(std::move(part));
    }
    if(hand.parts.empty())
        throw std::runtime_error("OpenVR controller has no renderable components");
}
}  // namespace lumaframevr::application::model_detail
