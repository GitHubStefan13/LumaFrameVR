#pragma once
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <cstdint>
#include <span>
#include <vector>
namespace lumaframevr {
struct SwapchainApi {
    PFN_xrEnumerateSwapchainImages enumerate{xrEnumerateSwapchainImages};
    PFN_xrAcquireSwapchainImage acquire{xrAcquireSwapchainImage};
    PFN_xrWaitSwapchainImage wait{xrWaitSwapchainImage};
    PFN_xrReleaseSwapchainImage release{xrReleaseSwapchainImage};
};
enum class ReleaseFailure { abandon, retain };
class AcquiredSwapchainImage {
   public:
    AcquiredSwapchainImage(XrSwapchain chain, const char* operation, SwapchainApi api = {});
    ~AcquiredSwapchainImage();
    AcquiredSwapchainImage(const AcquiredSwapchainImage&) = delete;
    AcquiredSwapchainImage& operator=(const AcquiredSwapchainImage&) = delete;
    AcquiredSwapchainImage(AcquiredSwapchainImage&& other) noexcept;
    AcquiredSwapchainImage& operator=(AcquiredSwapchainImage&& other) noexcept;
    bool active() const { return active_; }
    std::uint32_t index() const { return index_; }
    void wait(const char* operation);
    void release(const char* operation, ReleaseFailure failure = ReleaseFailure::abandon);
    XrResult release_unchecked() noexcept;
    void abandon() noexcept { active_ = false; }

   private:
    XrSwapchain chain_{};
    SwapchainApi api_;
    std::uint32_t index_{};
    bool active_{};
};
void require_swapchain_images(std::span<const XrSwapchainImageVulkan2KHR> images, const char* operation);
std::vector<XrSwapchainImageVulkan2KHR> swapchain_images(XrSwapchain chain, const char* operation, SwapchainApi api = {});
}  // namespace lumaframevr
