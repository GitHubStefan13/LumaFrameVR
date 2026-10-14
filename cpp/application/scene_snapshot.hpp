#pragma once
#include "panel_types.hpp"
#include "browser_previews.hpp"
#include "advanced_dialog_state.hpp"
#include "advanced_panels.hpp"
#include "transport.hpp"
#include "telemetry.hpp"
#include "viewer_settings.hpp"
namespace lumaframevr::application {
struct SceneSnapshot {
    std::array<std::optional<PanelKind>, 3> panels;
    bool playing{}, transport_visible{}, startup_visible{};
    std::size_t information_rows{};
    std::array<float, 2> bar_offset;
    std::array<std::optional<std::array<float, 2>>, 2> dialogs;
};
struct PanelSnapshot {
    const FolderBrowser& browser;
    ui::BrowserMode view;
    std::vector<TileShown> thumbnails;
    std::optional<PreviewDetails> preview_details;
    bool retiring{}, passthrough_enabled{}, passthrough_supported{};
    std::array<std::optional<Hit>, 2> hover;
    std::string error;
    ui::TransportState transport;
    const VideoPresentation& presentation;
    std::size_t selected_setting;
    AlignmentMode alignment_mode;
    const std::filesystem::path& path;
    const diagnostics::Telemetry& telemetry;
    const diagnostics::DeviceSample& device_sample;
    double xr_fps{}, target_fps{};
    ui::ViewerSettingsState viewer_settings;
    ui::LanguagePromptState language_prompt;
};
}  // namespace lumaframevr::application
