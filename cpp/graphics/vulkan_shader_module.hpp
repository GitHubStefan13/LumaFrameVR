#pragma once
#include <vulkan/vulkan.h>
#include <filesystem>
#include <vector>
namespace lumaframevr {
std::vector<std::uint32_t> read_spirv(const std::filesystem::path& path);
struct ShaderModuleApi {
    PFN_vkCreateShaderModule create{vkCreateShaderModule};
    PFN_vkDestroyShaderModule destroy{vkDestroyShaderModule};
};
class ScopedShaderModule {
   public:
    ScopedShaderModule(VkDevice device, const std::filesystem::path& path, const char* operation, ShaderModuleApi api = {});
    ~ScopedShaderModule();
    ScopedShaderModule(const ScopedShaderModule&) = delete;
    ScopedShaderModule& operator=(const ScopedShaderModule&) = delete;
    ScopedShaderModule(ScopedShaderModule&& other) noexcept;
    ScopedShaderModule& operator=(ScopedShaderModule&& other) noexcept;
    VkShaderModule handle() const { return module_; }
    void reset() noexcept;

   private:
    VkDevice device_{};
    VkShaderModule module_{};
    ShaderModuleApi api_;
};
}  // namespace lumaframevr
