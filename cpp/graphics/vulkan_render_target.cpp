#include "vulkan_render_target.hpp"
#include <stdexcept>
namespace lumaframevr {
VulkanRenderTarget::VulkanRenderTarget(std::shared_ptr<VulkanDevice> device, std::uint32_t width, std::uint32_t height, VkFormat format, bool depth)
    : device_(std::move(device)), extent_{width, height} {
    if(!width || !height)
        throw std::invalid_argument("Empty render target");
    owns_image_ = true;
    try {
        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.format = format;
        image.extent = {width, height, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        vulkan_check(vkCreateImage(device_->handle(), &image, nullptr, &image_), "create render target");
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_->handle(), image_, &requirements);
        VkMemoryAllocateInfo memory{};
        memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory.allocationSize = requirements.size;
        memory.memoryTypeIndex = device_->memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vulkan_check(vkAllocateMemory(device_->handle(), &memory, nullptr, &memory_), "allocate render target");
        vulkan_check(vkBindImageMemory(device_->handle(), image_, memory_, 0), "bind render target");
        if(depth)
            depth_ = std::make_unique<VulkanDepthAttachment>(device_, extent_);
        initialize(format, 0);
    } catch(...) {
        reset();
        throw;
    }
}
VulkanRenderTarget::VulkanRenderTarget(std::shared_ptr<VulkanDevice> device, VkImage image, VkExtent2D extent, VkFormat format, std::uint32_t eye, bool depth)
    : device_(std::move(device)), image_(image), extent_(extent) {
    try {
        if(depth)
            depth_ = std::make_unique<VulkanDepthAttachment>(device_, extent_);
        initialize(format, eye);
    } catch(...) {
        reset();
        throw;
    }
}
void VulkanRenderTarget::initialize(VkFormat format, std::uint32_t eye) {
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = image_;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = format;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, eye, 1};
    vulkan_check(vkCreateImageView(device_->handle(), &view, nullptr, &view_), "create render target view");
    VkAttachmentDescription attachment{};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    std::array<VkAttachmentDescription, 2> attachments{attachment, {}};
    attachments[1].format = depth_ ? depth_->format() : VK_FORMAT_D32_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    const VkAttachmentReference depth_reference{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    if(depth_)
        subpass.pDepthStencilAttachment = &depth_reference;
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if(depth_) {
        dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    VkRenderPassCreateInfo pass{};
    pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass.attachmentCount = depth_ ? 2 : 1;
    pass.pAttachments = attachments.data();
    pass.subpassCount = 1;
    pass.pSubpasses = &subpass;
    pass.dependencyCount = 1;
    pass.pDependencies = &dependency;
    vulkan_check(vkCreateRenderPass(device_->handle(), &pass, nullptr, &pass_), "create video render pass");
    VkFramebufferCreateInfo framebuffer{};
    framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer.renderPass = pass_;
    const std::array<VkImageView, 2> views{view_, depth_ ? depth_->view() : VK_NULL_HANDLE};
    framebuffer.attachmentCount = depth_ ? 2 : 1;
    framebuffer.pAttachments = views.data();
    framebuffer.width = extent_.width;
    framebuffer.height = extent_.height;
    framebuffer.layers = 1;
    vulkan_check(vkCreateFramebuffer(device_->handle(), &framebuffer, nullptr, &framebuffer_), "create video framebuffer");
}
void VulkanRenderTarget::begin(VkCommandBuffer command, float clear_alpha) const {
    std::array<VkClearValue, 2> clear{};
    clear[0].color.float32[3] = clear_alpha;
    clear[1].depthStencil = {1, 0};
    VkRenderPassBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = pass_;
    begin.framebuffer = framebuffer_;
    begin.renderArea.extent = extent_;
    begin.clearValueCount = depth_ ? 2 : 1;
    begin.pClearValues = clear.data();
    vkCmdBeginRenderPass(command, &begin, VK_SUBPASS_CONTENTS_INLINE);
}
void VulkanRenderTarget::end(VkCommandBuffer command) const {
    vkCmdEndRenderPass(command);
}
void VulkanRenderTarget::read(VkCommandBuffer command, const VulkanHostBuffer& output) const {
    if(output.size() < static_cast<VkDeviceSize>(extent_.width) * extent_.height * 4)
        throw std::invalid_argument("Readback buffer too small");
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image_;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {extent_.width, extent_.height, 1};
    vkCmdCopyImageToBuffer(command, image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, output.handle(), 1, &copy);
    VkMemoryBarrier host{};
    host.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, nullptr, 0, nullptr);
}
void VulkanRenderTarget::reset() noexcept {
    if(framebuffer_)
        vkDestroyFramebuffer(device_->handle(), framebuffer_, nullptr);
    if(pass_)
        vkDestroyRenderPass(device_->handle(), pass_, nullptr);
    if(view_)
        vkDestroyImageView(device_->handle(), view_, nullptr);
    depth_.reset();
    if(owns_image_ && image_)
        vkDestroyImage(device_->handle(), image_, nullptr);
    if(memory_)
        vkFreeMemory(device_->handle(), memory_, nullptr);
}
VulkanRenderTarget::~VulkanRenderTarget() {
    reset();
}
}  // namespace lumaframevr
