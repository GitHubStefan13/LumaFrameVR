#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
namespace lumaframevr {
void check_persistent_path(const std::filesystem::path& path);
std::optional<std::string> read_persistent_file(const std::filesystem::path& path, std::size_t limit);
void write_persistent_file(const std::filesystem::path& path, std::string_view bytes, const std::optional<std::string>& expected = {});
}  // namespace lumaframevr
