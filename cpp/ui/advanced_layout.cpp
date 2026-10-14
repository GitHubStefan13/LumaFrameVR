#include "advanced_layout.hpp"
#include "catalogue.hpp"
#include "font_style.hpp"
#include "icons.hpp"
#include "panel_theme.hpp"
#include <algorithm>
#include <cmath>
namespace lumaframevr::ui {
namespace {
using T = PanelTheme;
// Centres a run of text, which the pills and the chips both need. Inter is proportional, so the
// width has to be measured rather than assumed.
int centred_x(Rect area, std::string_view text, FontStyle style) {
    return area.x + std::max(0, (area.width - text_width(text, style)) / 2);
}
}  // namespace
Rect advanced_row(std::size_t index, int width) {
    return {18, 104 + static_cast<int>(index) * 50, width - 36, 46};
}
Rect information_row(std::size_t index, int width) {
    return {16, 40 + static_cast<int>(index) * 36, width - 32, 34};
}
Rect advanced_reset_button(int width) {
    return {width - 16 - control_width(i18n::tr("video.row.reset"), FontStyle::medium, true), 12, control_width(i18n::tr("video.row.reset"), FontStyle::medium, true), 36};
}
std::array<Rect, 2> advanced_mode_buttons(int width) {
    constexpr int gap = 10;
    // Each pill is as wide as the word inside it, up to the space the panel has: a longer
    // translation grows the pill instead of being clipped by it.
    const auto rotation = std::max(120, control_width(i18n::tr("video.tab.rotation"), FontStyle::medium, true) + 24);
    const auto position = std::max(120, control_width(i18n::tr("video.tab.position"), FontStyle::medium, true) + 24);
    const auto total = rotation + position + gap;
    const auto scale = static_cast<double>(width - 36) / total;
    const auto first = scale < 1.0 ? static_cast<int>(rotation * scale) : rotation;
    const auto second = width - 36 - first - gap;
    return {Rect{18, 56, first, 46}, Rect{18 + first + gap, 56, second, 46}};
}
Rect advanced_slider(Rect area) {
    return {area.x + 322, area.y + 10, area.width - 334, 26};
}
float advanced_fraction(Rect slider, int x) {
    return std::clamp(static_cast<float>(x - slider.x) / std::max(1, slider.width - 1), 0.0F, 1.0F);
}
Rect advanced_corner_dot(int width) {
    return {width - 44, 10, 32, 32};
}
std::string advanced_fit(std::string_view text, std::size_t columns) {
    const auto unicode = decode_utf8(text);
    if(unicode.size() <= columns)
        return std::string(text);
    const auto keep = columns > 0 ? columns - 1 : 0;
    return encode_utf8(U"\u2026" + unicode.substr(unicode.size() - keep));
}
void advanced_header(Canvas& canvas, std::string_view title, std::string_view trailing) {
    const Rect card{4, 4, canvas.width() - 8, canvas.height() - 8};
    canvas.rounded(card, 18, T::surface);
    draw_icon(canvas, {20, 13, 26, 26}, "refresh", T::primary);
    draw_text(canvas, 54, 12, title, T::on_surface, FontStyle::title);
    if(!trailing.empty())
        draw_text(canvas, card.right() - 20 - text_width(trailing, FontStyle::secondary), 20, trailing, T::on_surface_variant, FontStyle::secondary);
}
void advanced_line(Canvas& canvas, Rect area, std::string_view text, bool selected, Color color) {
    if(selected)
        canvas.rounded(area, 10, T::surface_container);
    draw_text(canvas, area.x + 8, area.y + (area.height - text_height(FontStyle::medium)) / 2, text, color, FontStyle::medium);
}
// A Material slider: a grey track, the filled part in the accent colour, and a round thumb with a
// ring around a white face so it still reads as something to grab against the fill.
void advanced_slider_track(Canvas& canvas, Rect band, float fraction) {
    const Rect track{band.x, band.y + (band.height - 10) / 2, band.width, 10};
    canvas.rounded(track, 5, T::track_inactive);
    const auto clamped = std::clamp(fraction, 0.0F, 1.0F);
    const auto fill = static_cast<int>(std::lround(track.width * clamped));
    if(fill > 0)
        canvas.rounded({track.x, track.y, fill, track.height}, 5, T::primary);
    const auto centre = track.x + static_cast<int>(std::lround(clamped * (track.width - 1)));
    canvas.rounded({centre - 9, track.y - 4, 18, 18}, 9, T::primary);
    canvas.rounded({centre - 6, track.y - 1, 12, 12}, 6, T::thumb_fill);
}
void advanced_draw_slider(Canvas& canvas, Rect area, std::string_view text, float fraction) {
    advanced_line(canvas, area, text, false, T::on_surface);
    advanced_slider_track(canvas, advanced_slider(area), fraction);
}
std::vector<PanelRow> draw_settings(Canvas& canvas, const VideoPresentation& p, std::size_t selected, std::string_view version, bool corner_dot) {
    canvas.clear();
    advanced_header(canvas, i18n::tr("video.title"), version);
    const auto fields = settings_fields(p);
    const auto areas = settings_layout(p, selected, canvas.width(), canvas.height());
    for(const auto& raw : areas) {
        const auto row = raw;
        const auto [label, value] = setting_label(fields[row.index], p);
        if(row.index == selected)
            canvas.rounded(row.area, 12, T::surface_container);
        draw_text(canvas, row.area.x + 12, row.area.y + 10, advanced_fit(label, 30), T::on_surface, FontStyle::medium);
        if(row.slider) {
            advanced_slider_track(canvas, *row.slider, std::clamp(*setting_fraction(p, fields[row.index]), 0.0F, 1.0F));
            const Rect chip{row.slider->x - 20 - control_width(value, FontStyle::medium) - 8, row.slider->y - 8, control_width(value, FontStyle::medium) + 16, 34};
            canvas.rounded(chip, 17, T::surface_raised);
            draw_text(canvas, centred_x(chip, value, FontStyle::medium), chip.y + 8, value, T::primary_text, FontStyle::medium);
        }
        for(const auto& button : row.options) {
            canvas.rounded(button.area, button.area.height / 2, button.option.selected ? T::primary : T::surface_container);
            draw_text(canvas, centred_x(button.area, button.option.label, FontStyle::medium), button.area.y + (button.area.height - text_height(FontStyle::medium)) / 2, button.option.label,
                      button.option.selected ? T::on_primary : T::on_surface, FontStyle::medium);
        }
    }
    if(corner_dot)
        canvas.rounded(advanced_corner_dot(canvas.width()), 16, T::on_surface_variant);
    return areas;
}
std::vector<PanelRow> draw_alignment(Canvas& canvas, const VideoPresentation& p, AlignmentMode mode, bool corner_dot) {
    canvas.clear();
    const auto orientation = mode == AlignmentMode::orientation;
    advanced_header(canvas, orientation ? i18n::tr("video.rotation.title") : i18n::tr("video.position.title"), "");
    const auto reset = advanced_reset_button(canvas.width());
    canvas.rounded(reset, reset.height / 2, T::surface_container);
    canvas.outline(reset, reset.height / 2, 1, T::outline);
    draw_icon(canvas, {reset.x + 14, reset.y + 8, 20, 20}, "refresh", T::on_surface);
    draw_text(canvas, reset.x + 42, reset.y + 9, i18n::tr("video.row.reset"), T::on_surface, FontStyle::medium);
    // The two modes are pills, the active one filled, each with the mark the reference gives it: a
    // box for orientation, glasses for stereo.
    const auto buttons = advanced_mode_buttons(canvas.width());
    const std::array labels{i18n::tr("video.tab.rotation"), i18n::tr("video.tab.position")};
    const std::array marks{"box", "glasses"};
    for(std::size_t i = 0; i < buttons.size(); ++i) {
        const bool active = (i == 0) == orientation;
        const auto ink = active ? T::on_primary : T::on_surface;
        canvas.rounded(buttons[i], buttons[i].height / 2, active ? T::primary : T::surface_container);
        const auto run = 26 + 10 + text_width(labels[i], FontStyle::medium);
        const auto start = buttons[i].x + std::max(0, (buttons[i].width - run) / 2);
        draw_icon(canvas, {start, buttons[i].y + 13, 22, 22}, marks[i], ink);
        draw_text(canvas, start + 32, buttons[i].y + 13, labels[i], ink, FontStyle::medium);
    }
    auto area = advanced_row(0, canvas.width());
    std::vector<PanelRow> areas{{area, 0, std::nullopt}};
    const auto sliders = alignment_sliders(p, mode);
    for(std::size_t i = 0; i < sliders.size(); ++i) {
        const auto& slider = sliders[i];
        area = advanced_row(i, canvas.width());
        const auto band = advanced_slider(area);
        draw_text(canvas, area.x + 6, area.y + 13, advanced_fit(slider.label, 20), T::on_surface, FontStyle::medium);
        const Rect chip{area.x + 170, area.y + 6, std::max(120, control_width(slider.value, FontStyle::medium) + 16), 34};
        canvas.rounded(chip, 17, T::surface_raised);
        draw_text(canvas, centred_x(chip, slider.value, FontStyle::medium), chip.y + 8, slider.value, slider.enabled ? T::primary_text : T::on_surface_variant, FontStyle::medium);
        if(slider.enabled)
            advanced_slider_track(canvas, band, slider.fraction);
        else
            canvas.rounded({band.x, band.y + 8, band.width, 10}, 5, T::track_inactive);
        areas.push_back({area, i, slider.enabled ? std::optional{band} : std::nullopt, slider.enabled});
    }
    if(corner_dot)
        canvas.rounded(advanced_corner_dot(canvas.width()), 16, T::on_surface_variant);
    return areas;
}
}  // namespace lumaframevr::ui
