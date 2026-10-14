#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>
namespace lumaframevr {
struct ImagePixels {
    int width{}, height{};
    std::vector<std::uint8_t> rgba;
};
inline constexpr std::int64_t maximum_image_pixels = 64LL * 1024 * 1024;
bool is_image_path(const std::filesystem::path& path);
ImagePixels read_image(const std::filesystem::path& path, const std::function<bool()>& cancelled = {});
}  // namespace lumaframevr
