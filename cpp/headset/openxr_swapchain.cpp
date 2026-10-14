#include "openxr_swapchain.hpp"
#include "openxr_runtime.hpp"
#include <stdexcept>
namespace lumaframevr {
void require_swapchain_images(std::span<const XrSwapchainImageVulkan2KHR> images, const char* operation) {
    if(images.empty())
        throw std::runtime_error(std::string(operation) + ": XR swapchain has no images");
}
std::vector<XrSwapchainImageVulkan2KHR> swapchain_images(XrSwapchain chain, const char* operation, SwapchainApi api) {
    if(!api.enumerate)
        throw std::invalid_argument("Missing swapchain enumeration API");
    std::uint32_t count{};
    xr_check(api.enumerate(chain, 0, &count, nullptr), operation);
    if(!count)
        throw std::runtime_error(std::string(operation) + ": XR swapchain has no images");
    std::vector<XrSwapchainImageVulkan2KHR> images(count);
    for(auto& image : images) image.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR;
    const auto capacity = count;
    xr_check(api.enumerate(chain, capacity, &count, reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())), operation);
    if(count > capacity)
        throw std::runtime_error(std::string(operation) + ": invalid XR swapchain image count");
    images.resize(count);
    require_swapchain_images(images, operation);
    return images;
}
}  // namespace lumaframevr
