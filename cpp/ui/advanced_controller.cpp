#include "advanced_panels.hpp"
#include <algorithm>
namespace lumaframevr::ui {
AdvancedSettingsController::AdvancedSettingsController(RememberedVideoSettings& settings, std::filesystem::path path, VideoPresentation& presentation)
    : settings_(settings), path_(std::move(path)), presentation_(presentation) {}
void AdvancedSettingsController::remember_if_changed(const VideoPresentation& before) {
    if(presentation_ != before)
        settings_.edit(path_, presentation_);
}
void AdvancedSettingsController::navigate(int vertical, int horizontal) {
    const auto before = presentation_;
    navigate_settings(presentation_, selected_, vertical, horizontal);
    remember_if_changed(before);
}
bool AdvancedSettingsController::click_setting(int x, int y) {
    return click_setting(x, y, settings_size[0], settings_size[1]);
}
bool AdvancedSettingsController::click_setting(int x, int y, int width, int height) {
    const auto fields = settings_fields(presentation_);
    selected_ = std::min(selected_, fields.size() - 1);
    for(const auto& row : settings_layout(presentation_, selected_, width, height)) {
        if(row.slider && row.slider->contains(x, y)) {
            selected_ = row.index;
            const auto before = presentation_;
            set_setting_fraction(presentation_, fields[selected_], advanced_fraction(*row.slider, x));
            remember_if_changed(before);
            return true;
        }
        for(const auto& button : row.options) {
            if(!button.area.contains(x, y))
                continue;
            selected_ = row.index;
            const auto before = presentation_;
            const auto field = fields[selected_];
            select_setting(presentation_, field, button.option.value);
            const auto after = settings_fields(presentation_);
            selected_ = std::min(static_cast<std::size_t>(std::find(after.begin(), after.end(), field) - after.begin()), after.size() - 1);
            remember_if_changed(before);
            return true;
        }
    }
    return false;
}
bool AdvancedSettingsController::drag_setting(SettingField field, Rect slider, int x) {
    const auto fields = settings_fields(presentation_);
    const auto found = std::find(fields.begin(), fields.end(), field);
    if(found == fields.end() || !setting_fraction(presentation_, field))
        return false;
    selected_ = static_cast<std::size_t>(found - fields.begin());
    const auto before = presentation_;
    set_setting_fraction(presentation_, field, advanced_fraction(slider, x));
    remember_if_changed(before);
    return true;
}
bool AdvancedSettingsController::click_alignment(int x, int y) {
    if(advanced_reset_button(alignment_size[0]).contains(x, y)) {
        reset_alignment();
        return true;
    }
    const auto buttons = advanced_mode_buttons(alignment_size[0]);
    for(std::size_t index = 0; index < buttons.size(); ++index)
        if(buttons[index].contains(x, y)) {
            mode_ = index == 0 ? AlignmentMode::orientation : AlignmentMode::position;
            return true;
        }
    for(std::size_t row = 0; row < 4; ++row)
        if(advanced_row(row).contains(x, y))
            return drag_alignment(row, x);
    return false;
}
bool AdvancedSettingsController::drag_alignment(std::size_t value_row, int x) {
    if(value_row >= 4 || !alignment_sliders(presentation_, mode_)[value_row].enabled)
        return false;
    stick_at_.reset();
    const auto before = presentation_;
    set_alignment_slider(presentation_, mode_, value_row, advanced_fraction(advanced_slider(advanced_row(value_row)), x));
    remember_if_changed(before);
    return true;
}
void AdvancedSettingsController::reset_alignment() {
    stick_at_.reset();
    const auto before = presentation_;
    presentation_.alignment = {};
    remember_if_changed(before);
}
}  // namespace lumaframevr::ui
