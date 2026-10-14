#include "application_assets.hpp"
#include "runtime_shaders.hpp"
#include <algorithm>
#include <cstdlib>
#include <vector>
#include <stdexcept>
namespace lumaframevr::application {
std::filesystem::path shader_directory() {
    std::vector<std::filesystem::path> candidates;
    if(const char* directory = std::getenv("LUMAFRAMEVR_SHADER_DIR"))
        candidates.emplace_back(directory);
    if(const char* root = std::getenv("LUMAFRAMEVR_PACKAGE_ROOT"))
        candidates.emplace_back(std::filesystem::path(root) / "shaders");
    const auto executable = std::filesystem::read_symlink("/proc/self/exe").parent_path();
    candidates.push_back(executable / "shaders");
    candidates.push_back(executable / "../share/lumaframevr/shaders");
    for(const auto& path : candidates)
        if(std::all_of(runtime_shaders.begin(), runtime_shaders.end(), [&path](std::string_view name) { return std::filesystem::is_regular_file(path / name); }))
            return path;
    throw std::runtime_error("Missing production shaders beside executable or in ../share/lumaframevr/shaders; set LUMAFRAMEVR_SHADER_DIR");
}

}  // namespace lumaframevr::application
