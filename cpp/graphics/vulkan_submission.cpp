#include "vulkan_submission.hpp"
#include <stdexcept>
#include <limits>
namespace lumaframevr {
VulkanSubmission::VulkanSubmission(std::shared_ptr<VulkanDevice> device) : device_(std::move(device)) {
    try {
        VkCommandPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.queueFamilyIndex = device_->queue_family();
        pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        vulkan_check(vkCreateCommandPool(device_->handle(), &pool, nullptr, &pool_), "create command pool");
        VkCommandBufferAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = pool_;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        vulkan_check(vkAllocateCommandBuffers(device_->handle(), &allocate, &command_), "allocate command buffer");
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vulkan_check(vkBeginCommandBuffer(command_, &begin), "begin command buffer");
        VkFenceCreateInfo fence{};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vulkan_check(vkCreateFence(device_->handle(), &fence, nullptr, &fence_), "create submission fence");
    } catch(...) {
        reset();
        throw;
    }
}
void VulkanSubmission::submit() {
    if(submitted_)
        throw std::logic_error("Vulkan submission already submitted");
    vulkan_check(vkEndCommandBuffer(command_), "end command buffer");
    VkSubmitInfo submission{};
    submission.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submission.commandBufferCount = 1;
    submission.pCommandBuffers = &command_;
    vulkan_check(vkQueueSubmit(device_->queue(), 1, &submission, fence_), "queue submit");
    submitted_ = true;
}
void VulkanSubmission::wait() {
    if(!submitted_)
        throw std::logic_error("Vulkan submission not submitted");
    vulkan_check(vkWaitForFences(device_->handle(), 1, &fence_, VK_TRUE, std::numeric_limits<std::uint64_t>::max()), "wait submission fence");
}
void VulkanSubmission::reset() noexcept {
    if(submitted_)
        vkWaitForFences(device_->handle(), 1, &fence_, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
    if(fence_)
        vkDestroyFence(device_->handle(), fence_, nullptr);
    if(pool_)
        vkDestroyCommandPool(device_->handle(), pool_, nullptr);
}
VulkanSubmission::~VulkanSubmission() {
    reset();
}
}  // namespace lumaframevr
