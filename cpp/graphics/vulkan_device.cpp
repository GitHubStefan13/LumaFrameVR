#include "vulkan_device.hpp"
#include <stdexcept>
#include <vector>
namespace lumaframevr {
void vulkan_check(VkResult result, const char* operation) {
    if(result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": Vulkan result " + std::to_string(result));
}
std::shared_ptr<VulkanDevice> VulkanDevice::create() {
    auto result = std::shared_ptr<VulkanDevice>(new VulkanDevice);
    VkApplicationInfo application{};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "LumaFrameVR";
    application.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo instance{};
    instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance.pApplicationInfo = &application;
    vulkan_check(vkCreateInstance(&instance, nullptr, &result->instance_), "create Vulkan instance");
    std::uint32_t count{};
    vulkan_check(vkEnumeratePhysicalDevices(result->instance_, &count, nullptr), "enumerate physical devices");
    std::vector<VkPhysicalDevice> devices(count);
    vulkan_check(vkEnumeratePhysicalDevices(result->instance_, &count, devices.data()), "enumerate physical devices");
    for(auto device : devices) {
        std::uint32_t families{};
        vkGetPhysicalDeviceQueueFamilyProperties(device, &families, nullptr);
        std::vector<VkQueueFamilyProperties> properties(families);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &families, properties.data());
        for(std::uint32_t i = 0; i < families; ++i) {
            if((properties[i].queueFlags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) != (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT))
                continue;
            result->physical_ = device;
            result->queue_family_ = i;
            break;
        }
        if(result->physical_)
            break;
    }
    if(!result->physical_)
        throw std::runtime_error("No Vulkan graphics/compute queue");
    const float priority = 1;
    VkDeviceQueueCreateInfo queue{};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueFamilyIndex = result->queue_family_;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    VkPhysicalDeviceSamplerYcbcrConversionFeatures conversion{};
    conversion.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &conversion;
    vkGetPhysicalDeviceFeatures2(result->physical_, &features);
    VkDeviceCreateInfo device{};
    device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device.pNext = &conversion;
    device.queueCreateInfoCount = 1;
    device.pQueueCreateInfos = &queue;
    vulkan_check(vkCreateDevice(result->physical_, &device, nullptr, &result->device_), "create Vulkan device");
    vkGetDeviceQueue(result->device_, result->queue_family_, 0, &result->queue_);
    return result;
}
std::shared_ptr<VulkanDevice> VulkanDevice::adopt(VkInstance instance, VkPhysicalDevice physical, VkDevice device, std::uint32_t family) {
    auto result = std::shared_ptr<VulkanDevice>(new VulkanDevice);
    result->instance_ = instance;
    result->physical_ = physical;
    result->device_ = device;
    result->queue_family_ = family;
    vkGetDeviceQueue(device, family, 0, &result->queue_);
    return result;
}
VulkanDevice::~VulkanDevice() {
    if(device_) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
    }
    if(instance_)
        vkDestroyInstance(instance_, nullptr);
}
std::string VulkanDevice::name() const {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_, &properties);
    return properties.deviceName;
}
std::uint32_t VulkanDevice::memory_type(std::uint32_t mask, VkMemoryPropertyFlags flags) const {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_, &properties);
    for(std::uint32_t i = 0; i < properties.memoryTypeCount; ++i)
        if((mask & (1u << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    throw std::runtime_error("No matching Vulkan memory type");
}
}  // namespace lumaframevr
