#include "test_registry.hpp"
#include "vulkan_shader_module.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <type_traits>
using namespace lumaframevr;
static void check(bool value, const char* reason) {
    if(!value)
        throw std::runtime_error(reason);
}
unsigned created{}, destroyed{};
std::vector<std::uintptr_t> destroyed_handles;
VkResult result = VK_SUCCESS;
VkResult VKAPI_PTR create(VkDevice, const VkShaderModuleCreateInfo* info, const VkAllocationCallbacks*, VkShaderModule* out) {
    check(info->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO && info->codeSize == 8 && info->pCode[0] == 0x07230203, "aligned SPIR-V passed intact");
    ++created;
    if(result == VK_SUCCESS)
        *out = reinterpret_cast<VkShaderModule>(static_cast<std::uintptr_t>(created));
    return result;
}
void VKAPI_PTR destroy(VkDevice, VkShaderModule value, const VkAllocationCallbacks*) {
    check(value != VK_NULL_HANDLE, "destroy only owned shader");
    ++destroyed;
    destroyed_handles.push_back(reinterpret_cast<std::uintptr_t>(value));
}
template <class F>
void fails(F f, const char* reason) {
    bool failed = false;
    try {
        f();
    } catch(const std::runtime_error&) {
        failed = true;
    }
    check(failed, reason);
}
int lumaframevr::tests::shader_module(int argc, char** argv) {
    try {
        check(argc == 2, "fixture directory required");
        const std::filesystem::path root(argv[1]);
        std::filesystem::create_directories(root);
        const auto valid = root / "valid.spv";
        const std::uint32_t words[]{0x07230203, 7};
        {
            std::ofstream out(valid, std::ios::binary);
            out.write(reinterpret_cast<const char*>(words), sizeof(words));
        }
        {
            std::ofstream out(root / "empty.spv", std::ios::binary);
        }
        {
            std::ofstream out(root / "unaligned.spv", std::ios::binary);
            out << "abc";
        }
        check(read_spirv(valid) == std::vector<std::uint32_t>({words[0], words[1]}), "reader retains exact words");
        fails([&] { read_spirv(root / "missing.spv"); }, "missing shader rejected");
        fails([&] { read_spirv(root / "empty.spv"); }, "empty shader rejected");
        fails([&] { read_spirv(root / "unaligned.spv"); }, "unaligned shader rejected");
        const ShaderModuleApi api{create, destroy};
        static_assert(!std::is_copy_constructible_v<ScopedShaderModule>);
        static_assert(std::is_nothrow_move_constructible_v<ScopedShaderModule>);
        {
            ScopedShaderModule first({}, valid, "first", api);
            const auto handle = first.handle();
            ScopedShaderModule moved(std::move(first));
            check(!first.handle() && moved.handle() == handle && destroyed == 0, "move transfers without cleanup");
            ScopedShaderModule second({}, valid, "second", api);
            second = std::move(moved);
            check(!moved.handle() && second.handle() == handle && destroyed == 1, "move assignment releases previous exactly once");
        }
        check(created == 2 && destroyed == 2, "success cleans exactly once");
        result = VK_ERROR_INITIALIZATION_FAILED;
        fails([&] { ScopedShaderModule shader({}, valid, "failed", api); }, "creation failure propagates");
        check(created == 3 && destroyed == 2, "failed creation does not destroy unowned module");
        result = VK_SUCCESS;
        fails(
                [&] {
                    ScopedShaderModule vertex({}, valid, "vertex", api);
                    result = VK_ERROR_INITIALIZATION_FAILED;
                    ScopedShaderModule fragment({}, valid, "fragment", api);
                },
                "second module failure propagates");
        check(created == 5 && destroyed == 3, "second-module failure releases vertex exactly once");
        result = VK_SUCCESS;
        fails(
                [&] {
                    ScopedShaderModule vertex({}, valid, "vertex", api);
                    ScopedShaderModule fragment({}, valid, "fragment", api);
                    throw std::runtime_error("pipeline failure");
                },
                "downstream failure propagates");
        check(created == 7 && destroyed == 5, "pipeline failure releases both modules exactly once");
        check(destroyed_handles == std::vector<std::uintptr_t>({2, 1, 4, 7, 6}), "exact owned handles destroyed in scope order, never failed handles");
        std::cout << "PASS shared SPIR-V reader and exact shader lifetime failures\n";
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
