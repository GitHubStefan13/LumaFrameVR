#pragma once
#include "video_presentation.hpp"
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
namespace lumaframevr::diagnostics {
struct ProfileOptions {
    std::filesystem::path file;
    std::vector<double> targets;
    double seconds{1}, poll_seconds{0.001};
    unsigned repeat{1};
    bool decoder{}, thumbnails{}, stats{};
    std::optional<std::array<int, 2>> render_size;
    VideoPresentation presentation;
};
std::optional<ProfileOptions> parse_profile_options(std::span<const std::string_view> arguments);
std::string_view profile_help();
}  // namespace lumaframevr::diagnostics
