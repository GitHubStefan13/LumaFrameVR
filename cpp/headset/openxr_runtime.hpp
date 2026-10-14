#pragma once
#include "vulkan_device.hpp"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <array>
#include <memory>
#include <string>
namespace lumaframevr {
void xr_check(XrResult result, const char* operation);
class OpenXrRuntime {
   public:
    static std::shared_ptr<OpenXrRuntime> create();
    ~OpenXrRuntime();
    OpenXrRuntime(const OpenXrRuntime&) = delete;
    OpenXrRuntime& operator=(const OpenXrRuntime&) = delete;
    // Mirrors OpenXrSession::abandon: after the runtime has gone, destroying the instance
    // calls into it and crashes. Leave it to process exit.
    void abandon() { abandoned_ = true; }
    XrInstance handle() const { return instance_; }
    XrSystemId system() const { return system_; }
    std::string name() const;
    bool passthrough_extension() const { return passthrough_extension_; }
    std::array<XrViewConfigurationView, 2> view_configuration() const;
    std::shared_ptr<VulkanDevice> create_graphics() const;
    template <class Function>
    Function function(const char* name) const {
        PFN_xrVoidFunction address{};
        xr_check(xrGetInstanceProcAddr(instance_, name, &address), name);
        return reinterpret_cast<Function>(address);
    }

   private:
    OpenXrRuntime() = default;
    XrInstance instance_{};
    bool abandoned_{};
    XrSystemId system_{};
    bool passthrough_extension_{};
};
}  // namespace lumaframevr
