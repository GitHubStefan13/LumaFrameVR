#include "openxr_runtime.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace lumaframevr {
std::shared_ptr<VulkanDevice> OpenXrRuntime::create_graphics() const {
    XrGraphicsRequirementsVulkan2KHR requirements{};
    requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR;
    xr_check(function<PFN_xrGetVulkanGraphicsRequirements2KHR>("xrGetVulkanGraphicsRequirements2KHR")(instance_, system_, &requirements), "get OpenXR Vulkan requirements");
    VkInstance instance{};
    VkDevice device{};
    try {
        VkApplicationInfo app{};
        app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName = "LumaFrameVR";
        app.apiVersion = VK_MAKE_API_VERSION(0, XR_VERSION_MAJOR(requirements.maxApiVersionSupported), XR_VERSION_MINOR(requirements.maxApiVersionSupported), 0);
        app.apiVersion = std::min(app.apiVersion, static_cast<std::uint32_t>(VK_API_VERSION_1_3));
        VkInstanceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        info.pApplicationInfo = &app;
        XrVulkanInstanceCreateInfoKHR create{};
        create.type = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR;
        create.systemId = system_;
        create.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
        create.vulkanCreateInfo = &info;
        VkResult status{};
        xr_check(function<PFN_xrCreateVulkanInstanceKHR>("xrCreateVulkanInstanceKHR")(instance_, &create, &instance, &status), "OpenXR create Vulkan instance");
        vulkan_check(status, "OpenXR Vulkan instance");
        XrVulkanGraphicsDeviceGetInfoKHR get{};
        get.type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR;
        get.systemId = system_;
        get.vulkanInstance = instance;
        VkPhysicalDevice physical{};
        xr_check(function<PFN_xrGetVulkanGraphicsDevice2KHR>("xrGetVulkanGraphicsDevice2KHR")(instance_, &get, &physical), "get OpenXR physical device");
        std::uint32_t count{};
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> queues(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, queues.data());
        std::uint32_t family = 0;
        constexpr VkQueueFlags required_queue = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        while(family < count && (queues[family].queueFlags & required_queue) != required_queue) ++family;
        if(family == count)
            throw std::runtime_error("OpenXR GPU has no graphics queue");
        vulkan_check(vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr), "enumerate Vulkan device extensions");
        std::vector<VkExtensionProperties> properties(count);
        vulkan_check(vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, properties.data()), "enumerate Vulkan device extensions");
        std::vector<const char*> extensions;
        for(const char* name :
            {VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME, VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME, VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME})
            if(std::any_of(properties.begin(), properties.end(), [&](const auto& p) { return std::strcmp(p.extensionName, name) == 0; }))
                extensions.push_back(name);
        VkPhysicalDeviceSamplerYcbcrConversionFeatures conversion{};
        conversion.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
        VkPhysicalDeviceFeatures2 features{};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &conversion;
        vkGetPhysicalDeviceFeatures2(physical, &features);
        if(!conversion.samplerYcbcrConversion)
            throw std::runtime_error("OpenXR GPU lacks sampler YCbCr conversion");
        const float priority = 1;
        VkDeviceQueueCreateInfo queue{};
        queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue.queueFamilyIndex = family;
        queue.queueCount = 1;
        queue.pQueuePriorities = &priority;
        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.pNext = &conversion;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue;
        device_info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        device_info.ppEnabledExtensionNames = extensions.data();
        XrVulkanDeviceCreateInfoKHR device_create{};
        device_create.type = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR;
        device_create.systemId = system_;
        device_create.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
        device_create.vulkanPhysicalDevice = physical;
        device_create.vulkanCreateInfo = &device_info;
        xr_check(function<PFN_xrCreateVulkanDeviceKHR>("xrCreateVulkanDeviceKHR")(instance_, &device_create, &device, &status), "OpenXR create Vulkan device");
        vulkan_check(status, "OpenXR Vulkan device");
        auto result = VulkanDevice::adopt(instance, physical, device, family);
        instance = VK_NULL_HANDLE;
        device = VK_NULL_HANDLE;
        return result;
    } catch(...) {
        if(device)
            vkDestroyDevice(device, nullptr);
        if(instance)
            vkDestroyInstance(instance, nullptr);
        throw;
    }
}
}  // namespace lumaframevr
