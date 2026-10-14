#include "model_openvr.hpp"
#include <stdexcept>
namespace lumaframevr::application::model_detail {
namespace {
std::string device_model(Session& session, TrackedDeviceIndex_t device) {
    ETrackedPropertyError error{};
    auto property = ETrackedDeviceProperty_Prop_RenderModelName_String;
    auto size = session.system->GetStringTrackedDeviceProperty(device, property, nullptr, 0, &error);
    if(!size || size > 4096)
        throw std::runtime_error("OpenVR render-model property unavailable: " + std::to_string(error));
    std::string name(size, '\0');
    auto actual = session.system->GetStringTrackedDeviceProperty(device, property, name.data(), size, &error);
    if(error != ETrackedPropertyError_TrackedProp_Success || actual != size || name.back() != '\0')
        throw std::runtime_error("OpenVR render-model property failed: " + std::to_string(error));
    name.pop_back();
    if(name.empty())
        throw std::runtime_error("OpenVR controller has empty render-model name");
    return name;
}
}  // namespace
void advance_hand(const std::shared_ptr<Session>& session, HandWork& hand, ETrackedControllerRole role) {
    try {
        auto device = session->system->GetTrackedDeviceIndexForControllerRole(role);
        if(device == k_unTrackedDeviceIndexInvalid || !session->system->IsTrackedDeviceConnected(device)) {
            hand = {};
            hand.snapshot.status = ModelLoadStatus::Searching;
            hand.snapshot.detail = "no connected controller for hand role";
            return;
        }
        auto name = device_model(*session, device);
        if(hand.name != name || hand.device != device) {
            hand = {};
            hand.name = std::move(name);
            hand.device = device;
            discover_parts(*session, hand);
            hand.snapshot.status = ModelLoadStatus::LoadingModel;
        }
        if(hand.snapshot.model || hand.snapshot.status == ModelLoadStatus::Error)
            return;
        auto status = ModelLoadStatus::Ready;
        for(auto& part : hand.parts) {
            auto current = load_part(session, hand, part);
            if(current == ModelLoadStatus::LoadingModel || (current == ModelLoadStatus::LoadingTexture && status != ModelLoadStatus::LoadingModel))
                status = current;
        }
        hand.snapshot.status = status;
        hand.snapshot.detail = "loading runtime controller components: " + hand.name;
        if(status != ModelLoadStatus::Ready)
            return;
        auto result = std::make_shared<RuntimeControllerModel>();
        result->name = hand.name;
        result->provenance = "SteamVR IVRRenderModels_006; neutral component state; device=" + std::to_string(device);
        result->grip_aligned = hand.grip_from_model.has_value();
        for(auto& part : hand.parts) result->parts.push_back(std::move(*part.result));
        hand.snapshot.model = std::move(result);
        hand.snapshot.status = hand.grip_from_model ? ModelLoadStatus::Ready : ModelLoadStatus::MissingGrip;
        hand.snapshot.detail = hand.grip_from_model ? "runtime controller parts are OpenXR grip-local" : "openxr_grip component unavailable; geometry remains raw-model-local; no offset invented";
    } catch(const std::exception& error) {
        hand.parts.clear();
        hand.snapshot = {ModelLoadStatus::Error, error.what(), {}};
    }
}
}  // namespace lumaframevr::application::model_detail
