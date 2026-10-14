#pragma once
#include "canvas.hpp"
#include "advanced_dialog_state.hpp"
#include "remembered_video_settings.hpp"
#include <optional>

namespace lumaframevr::ui {
enum class SettingKind { projection, format, stereo, stereo_format, eye_order, center, distortion, reset };
struct SettingField {
    SettingKind kind;
    std::size_t index{}, axis{};
    bool operator==(const SettingField&) const = default;
};
struct SettingOption {
    std::string label;
    int value;
    bool selected;
};
struct SettingButton {
    Rect area;
    SettingOption option;
};
struct PanelRow {
    Rect area;
    std::size_t index;
    std::optional<Rect> slider;
    bool enabled{true};
    std::vector<SettingButton> options{};
};
std::vector<SettingOption> setting_options(SettingField field, const VideoPresentation& presentation);
void select_setting(VideoPresentation& presentation, SettingField field, int value);
std::optional<float> setting_fraction(const VideoPresentation& presentation, SettingField field);
void set_setting_fraction(VideoPresentation& presentation, SettingField field, float fraction);
std::vector<PanelRow> settings_layout(const VideoPresentation& presentation, std::size_t selected, int width = settings_size[0], int height = settings_size[1]);
std::vector<SettingField> settings_fields(const VideoPresentation& presentation);
std::pair<std::string, std::string> setting_label(SettingField field, const VideoPresentation& presentation);
void adjust_setting(VideoPresentation& presentation, SettingField field, int direction);
void navigate_settings(VideoPresentation& presentation, std::size_t& selected, int vertical, int horizontal);
Rect advanced_row(std::size_t index, int width = settings_size[0]);
// The information panel's rows are their own geometry: its height is computed from a 36 pixel
// pitch, so it must not inherit the alignment panel's roomier rows or its text falls off the panel.
Rect information_row(std::size_t index, int width = 792);
Rect advanced_reset_button(int width = settings_size[0]);
std::array<Rect, 2> advanced_mode_buttons(int width = settings_size[0]);
Rect advanced_slider(Rect row);
float advanced_fraction(Rect slider, int x);
Rect advanced_corner_dot(int width = 792);
std::vector<PanelRow> draw_settings(Canvas& canvas, const VideoPresentation& presentation, std::size_t selected, std::string_view version = "", bool corner_dot = false);
std::vector<PanelRow> draw_alignment(Canvas& canvas, const VideoPresentation& presentation, AlignmentMode mode, bool corner_dot = false);
struct AlignmentSlider {
    std::string label, value;
    float fraction;
    bool enabled;
};
std::array<AlignmentSlider, 4> alignment_sliders(const VideoPresentation& presentation, AlignmentMode mode);
void set_alignment_slider(VideoPresentation& presentation, AlignmentMode mode, std::size_t row, float fraction);
void step_alignment(VideoPresentation& presentation, AlignmentMode mode, std::size_t row, int direction);
class AdvancedSettingsController {
   public:
    AdvancedSettingsController(RememberedVideoSettings& settings, std::filesystem::path path, VideoPresentation& presentation);
    void navigate(int vertical, int horizontal);
    bool click_setting(int x, int y);
    bool click_setting(int x, int y, int width, int height);
    bool drag_setting(SettingField field, Rect slider, int x);
    bool click_alignment(int x, int y);
    bool drag_alignment(std::size_t value_row, int x);
    void reset_alignment();
    bool adjust_sticks(const std::array<std::array<float, 2>, 2>& sticks, double now, float vertical_sign);
    void reset_stick_clock() { stick_at_.reset(); }
    std::size_t selected() const { return selected_; }
    AlignmentMode mode() const { return mode_; }

   private:
    void remember_if_changed(const VideoPresentation& before);
    RememberedVideoSettings& settings_;
    std::filesystem::path path_;
    VideoPresentation& presentation_;
    std::size_t selected_{};
    std::optional<double> stick_at_;
    AlignmentMode mode_{AlignmentMode::orientation};
};
}  // namespace lumaframevr::ui
