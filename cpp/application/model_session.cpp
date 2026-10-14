#include "model_openvr.hpp"
#include <dlfcn.h>
#include <stdexcept>
#include <unordered_map>
namespace lumaframevr::application::model_detail {
namespace {
template <class T>
T symbol(void* library, const char* name) {
    auto result = reinterpret_cast<T>(dlsym(library, name));
    if(!result)
        throw std::runtime_error(std::string("OpenVR missing symbol: ") + name);
    return result;
}
}  // namespace
Session::~Session() {
    // Shut OpenVR down before unloading the library. The model worker is joined
    // before this runs (see RuntimeControllerModels::Impl), so no thread is
    // inside OpenVR here.
    if(initialized && shutdown)
        shutdown();
    initialized = false;
    shutdown = nullptr;
    system = nullptr;
    models = nullptr;
    if(library)
        dlclose(library);
    library = nullptr;
}
void Session::open(const std::string& path) {
    library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if(!library)
        throw std::runtime_error(std::string("OpenVR unavailable: ") + dlerror());
    auto init = symbol<uint32_t (*)(EVRInitError*, EVRApplicationType, const char*)>(library, "VR_InitInternal2");
    shutdown = symbol<void (*)()>(library, "VR_ShutdownInternal");
    auto interface = symbol<void* (*)(const char*, EVRInitError*)>(library, "VR_GetGenericInterface");
    EVRInitError error{};
    init(&error, EVRApplicationType_VRApplication_Background, nullptr);
    if(error != EVRInitError_VRInitError_None)
        throw std::runtime_error("OpenVR background initialization failed: " + std::to_string(error));
    initialized = true;
    auto system_name = std::string("FnTable:") + IVRSystem_Version;
    system = static_cast<VR_IVRSystem_FnTable*>(interface(system_name.c_str(), &error));
    if(!system || error != EVRInitError_VRInitError_None)
        throw std::runtime_error("OpenVR IVRSystem_026 unavailable: " + std::to_string(error));
    auto models_name = std::string("FnTable:") + IVRRenderModels_Version;
    models = static_cast<VR_IVRRenderModels_FnTable*>(interface(models_name.c_str(), &error));
    if(!models || error != EVRInitError_VRInitError_None)
        throw std::runtime_error("OpenVR IVRRenderModels_006 unavailable: " + std::to_string(error));
    if(!system->GetTrackedDeviceIndexForControllerRole || !system->IsTrackedDeviceConnected || !system->GetStringTrackedDeviceProperty || !models->LoadRenderModel_Async || !models->FreeRenderModel ||
       !models->LoadTexture_Async || !models->FreeTexture || !models->GetComponentCount || !models->GetComponentName || !models->GetComponentRenderModelName || !models->GetComponentState)
        throw std::runtime_error("OpenVR incomplete model/property/component interface");
}
namespace {
std::mutex& cache_mutex() {
    static std::mutex mutex;
    return mutex;
}
std::unordered_map<std::string, std::weak_ptr<Session>>& cache() {
    static std::unordered_map<std::string, std::weak_ptr<Session>> sessions;
    return sessions;
}
}  // namespace
std::shared_ptr<Session> acquire_session(const std::string& path) {
    std::lock_guard lock(cache_mutex());
    if(auto session = cache()[path].lock())
        return session;
    auto session = std::make_shared<Session>();
    session->open(path);
    cache()[path] = session;
    return session;
}
void shutdown_sessions() {
    // Close the OpenVR connection now, while the process is still fully alive.
    // The cache is a static, so without this the Session destructor runs during
    // static destruction, after main returns, when the OpenVR library and the
    // SteamVR connection may already be gone; that is where the exit segfault
    // came from. The caller must have released its shared_ptr first.
    std::lock_guard lock(cache_mutex());
    cache().clear();
}
}  // namespace lumaframevr::application::model_detail
