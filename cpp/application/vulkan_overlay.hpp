#pragma once
#include "vulkan_device.hpp"
#include "application_scene.hpp"
#include "application_assets.hpp"
namespace lumaframevr::application {
class VulkanOverlay {
   public:
    VulkanOverlay(std::shared_ptr<VulkanDevice> device, VkRenderPass pass, const std::filesystem::path& directory, const char* fragment);
    ~VulkanOverlay();
    VulkanOverlay(const VulkanOverlay&) = delete;
    VulkanOverlay& operator=(const VulkanOverlay&) = delete;
    void draw(VkCommandBuffer command, VkExtent2D extent, std::span<const OverlayPayload> payloads) const;

   private:
    void reset() noexcept;
    std::shared_ptr<VulkanDevice> device_;
    VkPipelineLayout layout_{};
    VkPipeline pipeline_{};
};
}  // namespace lumaframevr::application
