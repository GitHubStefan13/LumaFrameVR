#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string_view>
namespace lumaframevr {
enum class MediaFileType { unsupported, image, video };
inline MediaFileType media_file_type(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    constexpr std::array<std::string_view, 10> images{".png", ".apng", ".jpg", ".jpeg", ".jpe", ".bmp", ".webp", ".gif", ".tiff", ".tif"};
    constexpr std::array<std::string_view, 15> videos{".mp4", ".mkv", ".avi", ".mov", ".webm", ".m4v", ".mpeg", ".mpg", ".ts", ".mts", ".m2ts", ".wmv", ".flv", ".ogv", ".vob"};
    if(std::find(images.begin(), images.end(), extension) != images.end())
        return MediaFileType::image;
    if(std::find(videos.begin(), videos.end(), extension) != videos.end())
        return MediaFileType::video;
    return MediaFileType::unsupported;
}
inline bool supported_media_file(const std::filesystem::path& path) {
    return media_file_type(path) != MediaFileType::unsupported;
}
}  // namespace lumaframevr
