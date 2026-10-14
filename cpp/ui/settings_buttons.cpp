#include "advanced_panels.hpp"
#include "font_style.hpp"
#include "catalogue.hpp"
#include <algorithm>
#include <stdexcept>
namespace lumaframevr::ui {
std::vector<SettingOption> setting_options(SettingField field, const VideoPresentation& p) {
    using K = SettingKind;
    switch(field.kind) {
        case K::projection:
            return {{i18n::tr("video.value.flat"), 0, p.projection == Projection::flat},
                    {"180", 1, p.projection == Projection::hemisphere},
                    {"360", 2, p.projection == Projection::sphere},
                    {i18n::tr("video.value.fisheye"), 3, p.projection == Projection::fisheye}};
        case K::format:
            if(p.projection != Projection::fisheye)
                return {{"180", 1, p.projection == Projection::hemisphere}, {"360", 2, p.projection == Projection::sphere}};
            return {{"180", 180, p.fisheye_degrees == 180}, {"190", 190, p.fisheye_degrees == 190}, {"200", 200, p.fisheye_degrees == 200}, {"220", 220, p.fisheye_degrees == 220}};
        case K::stereo:
            return {{i18n::tr("video.value.mono"), 0, p.stereo == Stereo::mono},
                    {i18n::tr("video.value.side_by_side"), 1, p.stereo == Stereo::side_by_side},
                    {i18n::tr("video.value.top_bottom"), 2, p.stereo == Stereo::top_bottom}};
        case K::stereo_format: {
            const auto format = p.stereo == Stereo::top_bottom ? p.tb_format : p.sbs_format;
            return {{i18n::tr("video.value.full"), 0, format == StereoFormat::full}, {i18n::tr("video.value.half"), 1, format == StereoFormat::half}};
        }
        case K::eye_order:
            return {{i18n::tr("video.value.normal"), 0, !p.swap_eyes}, {i18n::tr("video.value.reversed"), 1, p.swap_eyes}};
        case K::reset:
            return {{i18n::tr("video.value.restore"), 0, false}};
        case K::center: {
            const auto value = p.lens.centers.at(field.index).at(field.axis);
            return {{"0.000", 0, value == 0}, {"0.500", 500, value == 0.5F}, {"1.000", 1000, value == 1}};
        }
        case K::distortion: {
            const auto value = p.lens.distortion.at(field.index);
            return {{"-1.0000", -10000, value == -1}, {"0.0000", 0, value == 0}, {"1.0000", 10000, value == 1}};
        }
    }
    throw std::invalid_argument("Invalid settings field");
}
void select_setting(VideoPresentation& p, SettingField field, int value) {
    using K = SettingKind;
    switch(field.kind) {
        case K::projection:
            if(value < 0 || value > 3 || static_cast<int>(p.projection) == value)
                return;
            p.projection = static_cast<Projection>(value);
            if(p.projection == Projection::fisheye)
                p.fisheye_degrees = 190;
            break;
        case K::format:
            if(p.projection == Projection::fisheye)
                p.fisheye_degrees = static_cast<unsigned>(std::clamp(value, 1, 359));
            else if(value == 1 || value == 2)
                p.projection = static_cast<Projection>(value);
            break;
        case K::stereo:
            if(value >= 0 && value <= 2)
                p.stereo = static_cast<Stereo>(value);
            break;
        case K::stereo_format:
            if(value == 0 || value == 1)
                (p.stereo == Stereo::top_bottom ? p.tb_format : p.sbs_format) = static_cast<StereoFormat>(value);
            break;
        case K::eye_order:
            if(value == 0 || value == 1)
                p.swap_eyes = value != 0;
            break;
        case K::reset:
            p = VideoPresentation{};
            break;
        case K::center:
            p.lens.centers.at(field.index).at(field.axis) = std::clamp(value / 1000.0F, 0.0F, 1.0F);
            break;
        case K::distortion:
            p.lens.distortion.at(field.index) = std::clamp(value / 10000.0F, -1.0F, 1.0F);
            break;
    }
}
std::vector<PanelRow> settings_layout(const VideoPresentation& p, std::size_t selected, int width, int height) {
    const auto fields = settings_fields(p);
    std::vector<PanelRow> rows;
    for(std::size_t i = 0; i < fields.size(); ++i) {
        // The label sits on its own line with the control under it. At this type size a label
        // and a row of pills cannot share one line without running into each other.
        PanelRow row{{14, 0, width - 28, 88}, i, std::nullopt};
        const auto numeric = setting_fraction(p, fields[i]).has_value();
        constexpr int left = 24;
        int x = left, y = 36;
        for(const auto& option : setting_options(fields[i], p)) {
            const auto button_width = text_width(option.label, FontStyle::medium) + 30;
            if(x + button_width > width - 24 && x != left) {
                x = left;
                y += 58;
                row.area.height += 58;
            }
            row.options.push_back({{x, y, button_width, 44}, option});
            x += button_width + 10;
        }
        // A numeric row shows its presets and then its slider, and the slider gets a line of
        // its own: presets and a track on one line run into each other.
        if(numeric) {
            row.slider = Rect{252, y + 58, width - 276, 34};
            row.area.height += 58;
        }
        rows.push_back(row);
    }
    selected = std::min(selected, rows.size() - 1);
    std::size_t start = selected;
    int used = rows[selected].area.height + 2;
    while(start > 0 && used + rows[start - 1].area.height + 2 <= height - 46) {
        --start;
        used += rows[start].area.height + 2;
    }
    std::vector<PanelRow> visible;
    int y = 40;
    for(std::size_t i = start; i < rows.size() && y + rows[i].area.height <= height - 6; ++i) {
        auto row = rows[i];
        row.area.y = y;
        for(auto& button : row.options) button.area.y += y;
        if(row.slider)
            row.slider->y += y;
        visible.push_back(row);
        y += row.area.height + 10;
    }
    return visible;
}
}  // namespace lumaframevr::ui
