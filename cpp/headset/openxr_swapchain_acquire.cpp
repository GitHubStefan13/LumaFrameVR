#include "openxr_swapchain.hpp"
#include "openxr_runtime.hpp"
#include <utility>
namespace lumaframevr {
AcquiredSwapchainImage::AcquiredSwapchainImage(XrSwapchain chain, const char* operation, SwapchainApi api) : chain_(chain), api_(api) {
    if(!api_.acquire || !api_.wait || !api_.release)
        throw std::invalid_argument("Incomplete swapchain acquisition API");
    XrSwapchainImageAcquireInfo acquire{};
    acquire.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
    xr_check(api_.acquire(chain_, &acquire, &index_), operation);
    active_ = true;
}
AcquiredSwapchainImage::~AcquiredSwapchainImage() {
    release_unchecked();
}
AcquiredSwapchainImage::AcquiredSwapchainImage(AcquiredSwapchainImage&& other) noexcept : chain_(other.chain_), api_(other.api_), index_(other.index_), active_(std::exchange(other.active_, false)) {}
AcquiredSwapchainImage& AcquiredSwapchainImage::operator=(AcquiredSwapchainImage&& other) noexcept {
    if(this != &other) {
        release_unchecked();
        chain_ = other.chain_;
        api_ = other.api_;
        index_ = other.index_;
        active_ = std::exchange(other.active_, false);
    }
    return *this;
}
void AcquiredSwapchainImage::wait(const char* operation) {
    if(!active_)
        throw std::logic_error("Cannot wait for unacquired swapchain image");
    XrSwapchainImageWaitInfo wait{};
    wait.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
    wait.timeout = XR_INFINITE_DURATION;
    xr_check(api_.wait(chain_, &wait), operation);
}
XrResult AcquiredSwapchainImage::release_unchecked() noexcept {
    if(!std::exchange(active_, false))
        return XR_SUCCESS;
    XrSwapchainImageReleaseInfo release{};
    release.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
    return api_.release(chain_, &release);
}
void AcquiredSwapchainImage::release(const char* operation, ReleaseFailure failure) {
    if(!active_)
        return;
    const auto result = release_unchecked();
    if(XR_FAILED(result) && failure == ReleaseFailure::retain)
        active_ = true;
    xr_check(result, operation);
}
}  // namespace lumaframevr
