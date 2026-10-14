#include "vulkan_shader_module.hpp"
#include "vulkan_device.hpp"
#include <fstream>
#include <utility>
namespace lumaframevr {
std::vector<std::uint32_t> read_spirv(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if(!file)
        throw std::runtime_error("Cannot read SPIR-V: " + path.string());
    const auto size = file.tellg();
    if(size <= 0 || size % sizeof(std::uint32_t))
        throw std::runtime_error("Invalid SPIR-V size: " + path.string());
    std::vector<std::uint32_t> words(static_cast<std::size_t>(size) / sizeof(std::uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(words.data()), size);
    if(!file)
        throw std::runtime_error("Incomplete SPIR-V read: " + path.string());
    return words;
}
ScopedShaderModule::ScopedShaderModule(VkDevice device, const std::filesystem::path& path, const char* operation, ShaderModuleApi api) : device_(device), api_(api) {
    if(!api_.create || !api_.destroy)
        throw std::invalid_argument("Incomplete shader module API");
    const auto words = read_spirv(path);
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = words.size() * sizeof(std::uint32_t);
    info.pCode = words.data();
    vulkan_check(api_.create(device_, &info, nullptr, &module_), operation);
}
ScopedShaderModule::~ScopedShaderModule() {
    reset();
}
ScopedShaderModule::ScopedShaderModule(ScopedShaderModule&& other) noexcept : device_(other.device_), module_(std::exchange(other.module_, VK_NULL_HANDLE)), api_(other.api_) {}
ScopedShaderModule& ScopedShaderModule::operator=(ScopedShaderModule&& other) noexcept {
    if(this != &other) {
        reset();
        device_ = other.device_;
        module_ = std::exchange(other.module_, VK_NULL_HANDLE);
        api_ = other.api_;
    }
    return *this;
}
void ScopedShaderModule::reset() noexcept {
    if(module_)
        api_.destroy(device_, std::exchange(module_, VK_NULL_HANDLE), nullptr);
}
}  // namespace lumaframevr
