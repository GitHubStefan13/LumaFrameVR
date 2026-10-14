#pragma once
#include "canvas.hpp"
#include "vulkan_device.hpp"
namespace lumaframevr {
void upload_panel_image(std::shared_ptr<VulkanDevice> device, VkImage image, const ui::Canvas& canvas);
}
