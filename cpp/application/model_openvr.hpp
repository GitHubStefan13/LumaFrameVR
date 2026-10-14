#pragma once
#include "model_runtime.hpp"
#include <openvr_capi.h>
#include <mutex>
namespace lumaframevr::application::model_detail {
using Matrix = std::array<float, 16>;
struct Session {
    void* library{};
    void (*shutdown)(){};
    bool initialized{};
    VR_IVRSystem_FnTable* system{};
    VR_IVRRenderModels_FnTable* models{};
    std::mutex api_mutex;
    ~Session();
    void open(const std::string& path);
};
std::shared_ptr<Session> acquire_session(const std::string& path);
// Close every cached OpenVR session now, on the calling thread. Call this before
// the process exits, so the OpenVR shutdown does not run during static
// destruction (which segfaults once SteamVR has gone).
void shutdown_sessions();
Matrix identity();
Matrix matrix_from(const HmdMatrix34_t& source);
Matrix inverse_rigid(const Matrix& source);
Matrix multiply(const Matrix& a, const Matrix& b);
void transform_vertices(ControllerModel& model, const Matrix& transform);
struct PartWork {
    std::string name, component;
    Matrix model_from_part;
    std::shared_ptr<RenderModel_t> mesh;
    std::optional<ControllerModel> result;
    bool visible{true};
};
struct HandWork {
    std::string name;
    TrackedDeviceIndex_t device{k_unTrackedDeviceIndexInvalid};
    std::vector<PartWork> parts;
    std::optional<Matrix> grip_from_model;
    HandModelSnapshot snapshot;
};
void discover_parts(Session& session, HandWork& hand);
ModelLoadStatus load_part(const std::shared_ptr<Session>& session, HandWork& hand, PartWork& part);
void advance_hand(const std::shared_ptr<Session>& session, HandWork& hand, ETrackedControllerRole role);
}  // namespace lumaframevr::application::model_detail
