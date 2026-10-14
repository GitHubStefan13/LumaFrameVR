#include "openxr_runtime.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace lumaframevr {
void xr_check(XrResult result, const char* operation) {
    if(XR_FAILED(result))
        throw std::runtime_error(std::string(operation) + ": OpenXR result " + std::to_string(result));
}
std::shared_ptr<OpenXrRuntime> OpenXrRuntime::create() {
    auto result = std::shared_ptr<OpenXrRuntime>(new OpenXrRuntime);
    std::uint32_t count{};
    xr_check(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr), "enumerate OpenXR extensions");
    std::vector<XrExtensionProperties> properties(count);
    for(auto& property : properties) property.type = XR_TYPE_EXTENSION_PROPERTIES;
    xr_check(xrEnumerateInstanceExtensionProperties(nullptr, count, &count, properties.data()), "enumerate OpenXR extensions");
    if(std::none_of(properties.begin(), properties.end(), [](const auto& p) { return std::strcmp(p.extensionName, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME) == 0; }))
        throw std::runtime_error("OpenXR runtime lacks XR_KHR_vulkan_enable2");
    std::vector<const char*> extensions{XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
    result->passthrough_extension_ = std::any_of(properties.begin(), properties.end(), [](const auto& p) { return std::strcmp(p.extensionName, XR_FB_PASSTHROUGH_EXTENSION_NAME) == 0; });
    if(result->passthrough_extension_)
        extensions.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);
    XrInstanceCreateInfo info{};
    info.type = XR_TYPE_INSTANCE_CREATE_INFO;
    std::strncpy(info.applicationInfo.applicationName, "LumaFrameVR", XR_MAX_APPLICATION_NAME_SIZE - 1);
    std::strncpy(info.applicationInfo.engineName, "LumaFrameVR", XR_MAX_ENGINE_NAME_SIZE - 1);
    info.applicationInfo.applicationVersion = 723;
    info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    info.enabledExtensionNames = extensions.data();
    // Some runtimes (SteamVR among them) reject the newest API version. Fall back
    // to 1.0, which every runtime supports, instead of failing outright.
    XrResult created = xrCreateInstance(&info, &result->instance_);
    if(created == XR_ERROR_API_VERSION_UNSUPPORTED) {
        info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
        created = xrCreateInstance(&info, &result->instance_);
    }
    xr_check(created, "create OpenXR instance");
    XrSystemGetInfo system{};
    system.type = XR_TYPE_SYSTEM_GET_INFO;
    system.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    xr_check(xrGetSystem(result->instance_, &system, &result->system_), "get head-mounted OpenXR system");
    result->view_configuration();
    return result;
}
OpenXrRuntime::~OpenXrRuntime() {
    if(instance_ && !abandoned_)
        xrDestroyInstance(instance_);
}
std::string OpenXrRuntime::name() const {
    XrInstanceProperties properties{};
    properties.type = XR_TYPE_INSTANCE_PROPERTIES;
    xr_check(xrGetInstanceProperties(instance_, &properties), "get OpenXR runtime properties");
    return properties.runtimeName;
}
std::array<XrViewConfigurationView, 2> OpenXrRuntime::view_configuration() const {
    std::uint32_t count{};
    xr_check(xrEnumerateViewConfigurationViews(instance_, system_, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &count, nullptr), "enumerate stereo views");
    if(count != 2)
        throw std::runtime_error("OpenXR primary stereo must have exactly two views");
    std::array<XrViewConfigurationView, 2> views{};
    for(auto& view : views) view.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    xr_check(xrEnumerateViewConfigurationViews(instance_, system_, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &count, views.data()), "enumerate stereo views");
    return views;
}
}  // namespace lumaframevr
