#pragma once
#include "media_source.hpp"
#include "video_hints.hpp"
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace lumaframevr {
enum class ApplicationAction { run, help, version };
struct ApplicationOptions {
    ApplicationAction action{ApplicationAction::run};
    std::filesystem::path file;
    std::optional<MediaSource> source;
    VideoPresentation presentation;
    VideoHints overrides;
    std::optional<std::filesystem::path> viewer_state;
    bool restore_last_folder{};
    bool probe{}, render_probe{}, hud{}, stats{};
    std::optional<double> seconds;
    std::optional<std::string> decoder;
    std::optional<std::filesystem::path> snapshot, hud_snapshot;
    MediaSource input() const;
    void apply_decoder_option() const;
};
ApplicationOptions parse_application_options(std::span<const std::string_view> arguments);
std::string_view application_help();
}  // namespace lumaframevr
