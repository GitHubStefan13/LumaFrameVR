#include "vulkan_host_buffer.hpp"
#include <stdexcept>
namespace lumaframevr {
VulkanHostBuffer::VulkanHostBuffer(std::shared_ptr<VulkanDevice> device, VkDeviceSize size, VkBufferUsageFlags extra_usage) : device_(std::move(device)), size_(size) {
    if(!size)
        throw std::invalid_argument("Empty Vulkan buffer");
    try {
        VkBufferCreateInfo buffer{};
        buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer.size = size;
        buffer.usage = extra_usage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vulkan_check(vkCreateBuffer(device_->handle(), &buffer, nullptr, &buffer_), "create buffer");
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device_->handle(), buffer_, &requirements);
        VkMemoryAllocateInfo memory{};
        memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory.allocationSize = requirements.size;
        memory.memoryTypeIndex = device_->memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vulkan_check(vkAllocateMemory(device_->handle(), &memory, nullptr, &memory_), "allocate host buffer");
        vulkan_check(vkBindBufferMemory(device_->handle(), buffer_, memory_, 0), "bind host buffer");
        vulkan_check(vkMapMemory(device_->handle(), memory_, 0, size, 0, &mapped_), "map host buffer");
    } catch(...) {
        reset();
        throw;
    }
}
void VulkanHostBuffer::reset() noexcept {
    if(mapped_)
        vkUnmapMemory(device_->handle(), memory_);
    if(buffer_)
        vkDestroyBuffer(device_->handle(), buffer_, nullptr);
    if(memory_)
        vkFreeMemory(device_->handle(), memory_, nullptr);
}
VulkanHostBuffer::~VulkanHostBuffer() {
    reset();
}
}  // namespace lumaframevr
