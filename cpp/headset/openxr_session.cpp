#include "openxr_session.hpp"
#include "openxr_swapchain.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
namespace lumaframevr {
OpenXrSession::OpenXrSession(std::shared_ptr<OpenXrRuntime> runtime) : runtime_(std::move(runtime)), graphics_(runtime_->create_graphics()) {
    try {
        XrGraphicsBindingVulkan2KHR binding{};
        binding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR;
        binding.instance = graphics_->instance();
        binding.physicalDevice = graphics_->physical();
        binding.device = graphics_->handle();
        binding.queueFamilyIndex = graphics_->queue_family();
        XrSessionCreateInfo create{};
        create.type = XR_TYPE_SESSION_CREATE_INFO;
        create.next = &binding;
        create.systemId = runtime_->system();
        xr_check(xrCreateSession(runtime_->handle(), &create, &session_), "create OpenXR Vulkan session");
        XrReferenceSpaceCreateInfo space{};
        space.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
        space.poseInReferenceSpace.orientation.w = 1;
        space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        xr_check(xrCreateReferenceSpace(session_, &space, &local_), "create local reference space");
        space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        xr_check(xrCreateReferenceSpace(session_, &space, &view_), "create view reference space");
        std::uint32_t count{};
        xr_check(xrEnumerateSwapchainFormats(session_, 0, &count, nullptr), "enumerate XR swapchain formats");
        std::vector<std::int64_t> formats(count);
        xr_check(xrEnumerateSwapchainFormats(session_, count, &count, formats.data()), "enumerate XR swapchain formats");
        for(auto candidate : {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB})
            if(std::find(formats.begin(), formats.end(), candidate) != formats.end()) {
                format_ = candidate;
                break;
            }
        if(!format_)
            throw std::runtime_error("OpenXR compositor exposes no sRGB RGBA format");
        auto views = runtime_->view_configuration();
        extent_ = {std::max(views[0].recommendedImageRectWidth, views[1].recommendedImageRectWidth), std::max(views[0].recommendedImageRectHeight, views[1].recommendedImageRectHeight)};
        XrSwapchainCreateInfo swapchain{};
        swapchain.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        swapchain.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain.format = format_;
        swapchain.sampleCount = 1;
        swapchain.width = extent_.width;
        swapchain.height = extent_.height;
        swapchain.faceCount = 1;
        swapchain.arraySize = 2;
        swapchain.mipCount = 1;
        xr_check(xrCreateSwapchain(session_, &swapchain, &swapchain_), "create stereo swapchain");
        images_ = swapchain_images(swapchain_, "enumerate stereo swapchain images");
    } catch(...) {
        reset();
        throw;
    }
}
void OpenXrSession::request_exit() {
    exit_requested_ = true;
    if(!session_)
        return;
    const XrResult result = xrRequestExitSession(session_);
    if(XR_FAILED(result))
        std::cerr << "XR: the runtime did not accept the exit request (" << result << ")\n";
}

void OpenXrSession::end_session() {
    if(!session_ || !running_)
        return;
    const XrResult result = xrEndSession(session_);
    if(XR_FAILED(result))
        std::cerr << "XR: ending the session reported " << result << "\n";
    running_ = false;
}

void leave_session(OpenXrSession& session) {
    if(session.instance_lost()) {
        std::cerr << "XR: instance lost; dropping the session without touching the runtime\n";
        session.abandon();
        if(auto handle = session.runtime())
            handle->abandon();
        return;
    }
    session.end_session();
    const char* destroy_on_exit = std::getenv("LUMAFRAMEVR_DESTROY_XR_SESSION_ON_EXIT");
    if(destroy_on_exit && *destroy_on_exit && *destroy_on_exit != '0') {
        std::cerr << "XR: ending the session and destroying the session and instance\n";
        return;
    }
    std::cerr << "XR: ending the session; the session itself is left to the runtime\n";
    session.abandon();
    if(auto handle = session.runtime())
        handle->abandon();
}

void OpenXrSession::reset() noexcept {
    if(!abandoned_) {
        if(swapchain_)
            xrDestroySwapchain(swapchain_);
        if(view_)
            xrDestroySpace(view_);
        if(local_)
            xrDestroySpace(local_);
        if(session_)
            xrDestroySession(session_);
    }
    swapchain_ = XR_NULL_HANDLE;
    view_ = XR_NULL_HANDLE;
    local_ = XR_NULL_HANDLE;
    session_ = XR_NULL_HANDLE;
}
OpenXrSession::~OpenXrSession() {
    reset();
}
bool OpenXrSession::poll() {
    for(;;) {
        XrEventDataBuffer event{};
        event.type = XR_TYPE_EVENT_DATA_BUFFER;
        auto result = xrPollEvent(runtime_->handle(), &event);
        if(result == XR_EVENT_UNAVAILABLE)
            break;
        xr_check(result, "poll OpenXR event");
        if(event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
            std::cerr << "XR: instance loss pending\n";
            instance_lost_ = true;
            exiting_ = true;
            running_ = false;
        }
        if(event.type == XR_TYPE_EVENT_DATA_PASSTHROUGH_STATE_CHANGED_FB) {
            const auto* camera = reinterpret_cast<const XrEventDataPassthroughStateChangedFB*>(&event);
            if(camera->flags &
               (XR_PASSTHROUGH_STATE_CHANGED_NON_RECOVERABLE_ERROR_BIT_FB | XR_PASSTHROUGH_STATE_CHANGED_REINIT_REQUIRED_BIT_FB | XR_PASSTHROUGH_STATE_CHANGED_RECOVERABLE_ERROR_BIT_FB))
                passthrough_fault_ = true;
            if(camera->flags & XR_PASSTHROUGH_STATE_CHANGED_RESTORED_ERROR_BIT_FB)
                passthrough_fault_ = false;
        }
        if(event.type != XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
            continue;
        auto changed = reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
        if(changed->session != session_)
            continue;
        state_ = changed->state;
        if(state_ == XR_SESSION_STATE_READY) {
            XrSessionBeginInfo begin{};
            begin.type = XR_TYPE_SESSION_BEGIN_INFO;
            begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            xr_check(xrBeginSession(session_, &begin), "begin XR session");
            running_ = true;
        } else if(state_ == XR_SESSION_STATE_STOPPING) {
            std::cerr << "XR: runtime asked to stop; ending session\n";
            xr_check(xrEndSession(session_), "end XR session");
            running_ = false;
        } else if(state_ == XR_SESSION_STATE_EXITING) {
            std::cerr << "XR: runtime asked to exit; destroying the session now\n";
            exiting_ = true;
            running_ = false;
        } else if(state_ == XR_SESSION_STATE_LOSS_PENDING) {
            instance_lost_ = true;
            exiting_ = true;
            running_ = false;
        }
    }
    return !exiting_;
}
std::unique_ptr<OpenXrFrame> OpenXrSession::begin_frame() {
    if(!running_)
        throw std::logic_error("Cannot begin frame on inactive XR session");
    return std::make_unique<OpenXrFrame>(*this);
}
}  // namespace lumaframevr
