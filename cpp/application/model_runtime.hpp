#pragma once
#include "model_data.hpp"
#include <array>
#include <memory>
#include <string>
namespace lumaframevr::application {
enum class ModelLoadStatus { Starting, Unavailable, Searching, LoadingModel, LoadingTexture, Ready, MissingGrip, Error };
struct RuntimeControllerModel {
    std::string name, provenance;
    std::vector<ControllerModel> parts;
    bool grip_aligned{};
};
struct HandModelSnapshot {
    ModelLoadStatus status{ModelLoadStatus::Starting};
    std::string detail;
    std::shared_ptr<const RuntimeControllerModel> model;
};
struct RuntimeModelSnapshot {
    std::array<HandModelSnapshot, 2> hands;
};
class RuntimeControllerModels {
   public:
    explicit RuntimeControllerModels(std::string library_path = "libopenvr_api.so");
    ~RuntimeControllerModels();
    RuntimeControllerModels(const RuntimeControllerModels&) = delete;
    RuntimeControllerModels& operator=(const RuntimeControllerModels&) = delete;
    RuntimeModelSnapshot poll() const;
    // Stop the background worker and wait for it. Call this before the OpenXR and
    // OpenVR sessions are torn down, so the worker is not inside OpenVR while the
    // library is unloaded.
    void shutdown();

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace lumaframevr::application
