#pragma once
#include "canvas.hpp"
#include "catalogue.hpp"
#include "language_prompt.hpp"
#include "update_state.hpp"
#include <optional>
#include <string>
#include <vector>

namespace lumaframevr::ui {
struct ViewerSettingsState {
    bool open{}, show_hidden{true}, save_thumbnails{}, remember_last_state{true};
    std::size_t selected{};
    std::string language{"en"};
    std::string error;
    // The licence files that shipped in the package, listed by name. Filled from the installation
    // rather than written into the code, so the list cannot drift from what was shipped.
    std::vector<std::string> licences{};
    UpdateState update{};
    std::size_t update_page{};
};
struct UpdateRowLayout {
    Rect area, button, more;
    std::vector<std::string> button_lines, more_lines, lines;
    std::size_t pages{1};
};
UpdateRowLayout update_row_layout(Rect area, const UpdateState& state, std::size_t page);
struct ViewerSettingTarget {
    Rect area;
    std::size_t index;
    bool value;
    std::string language;
};
Rect viewer_settings_card(int width, int height);
std::vector<ViewerSettingTarget> viewer_setting_targets(int width, int height, const std::vector<i18n::Language>& languages, const ViewerSettingsState& state = {});
std::optional<ViewerSettingTarget> viewer_setting_at(int width, int height, const std::vector<i18n::Language>& languages, int x, int y, const ViewerSettingsState& state = {});
void draw_viewer_settings(Canvas& canvas, const ViewerSettingsState& state, const std::vector<i18n::Language>& languages, std::string_view version = "");
}  // namespace lumaframevr::ui
