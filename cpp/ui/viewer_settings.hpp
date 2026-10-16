#pragma once
#include "canvas.hpp"
#include "catalogue.hpp"
#include "language_prompt.hpp"
#include "update_state.hpp"
#include <optional>
#include <string>
#include <vector>

namespace lumaframevr::ui {
// The rows the controller addresses by index: the toggles in the order the panel draws them, then
// the fixed rows under them. Both the panel and the controller act on these, so they live in one
// place; a row added here is a row every one of those sites already knows about.
inline constexpr std::size_t viewer_setting_hidden = 0;
inline constexpr std::size_t viewer_setting_file_sizes = 1;
inline constexpr std::size_t viewer_setting_thumbnails = 2;
inline constexpr std::size_t viewer_setting_remember = 3;
inline constexpr std::size_t viewer_setting_toggles = 4;
// The update row is selectable, and left/right pages it; the update button under it starts the
// check. They were one index once, which is why a click on Close used to be indistinguishable from
// the row that runs an update.
inline constexpr std::size_t viewer_setting_update_row = viewer_setting_toggles;
inline constexpr std::size_t viewer_setting_language = viewer_setting_toggles + 1;
inline constexpr std::size_t viewer_setting_update = viewer_setting_toggles + 2;
inline constexpr std::size_t viewer_setting_update_more = viewer_setting_toggles + 3;
inline constexpr std::size_t viewer_setting_close = viewer_setting_toggles + 4;
struct ViewerSettingsState {
    bool open{}, show_hidden{true}, show_file_sizes{}, save_thumbnails{}, remember_last_state{true};
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
