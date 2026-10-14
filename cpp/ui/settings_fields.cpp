#include "advanced_panels.hpp"
#include "catalogue.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace lumaframevr::ui {
std::vector<SettingField> settings_fields(const VideoPresentation& p) {
    using K = SettingKind;
    std::vector<SettingField> fields{{K::projection}};
    if(p.projection != Projection::flat)
        fields.push_back({K::format});
    fields.push_back({K::stereo});
    if(p.projection == Projection::flat && p.stereo != Stereo::mono)
        fields.push_back({K::stereo_format});
    fields.push_back({K::eye_order});
    if(p.projection == Projection::fisheye) {
        for(std::size_t eye = 0; eye < 2; ++eye)
            for(std::size_t axis = 0; axis < 2; ++axis) fields.push_back({K::center, eye, axis});
        for(std::size_t i = 0; i < 4; ++i) fields.push_back({K::distortion, i});
    }
    fields.push_back({K::reset});
    return fields;
}
std::pair<std::string, std::string> setting_label(SettingField field, const VideoPresentation& p) {
    using K = SettingKind;
    std::ostringstream value;
    switch(field.kind) {
        case K::projection:
            return {i18n::tr("video.row.projection"), p.projection == Projection::flat      ? i18n::tr("video.value.flat")
                                                      : p.projection == Projection::fisheye ? i18n::tr("video.value.fisheye")
                                                                                            : i18n::tr("video.value.equirectangular")};
        case K::format:
            return {i18n::tr("video.row.format"), p.projection == Projection::fisheye      ? i18n::tr("video.value.degrees_short", {{"degrees", std::to_string(p.fisheye_degrees)}})
                                                  : p.projection == Projection::hemisphere ? "180"
                                                  : p.projection == Projection::sphere     ? "360"
                                                                                           : i18n::tr("video.value.none")};
        case K::stereo:
            return {i18n::tr("video.row.stereo"), p.stereo == Stereo::mono           ? i18n::tr("video.value.mono")
                                                  : p.stereo == Stereo::side_by_side ? i18n::tr("video.value.side_by_side")
                                                                                     : i18n::tr("video.value.top_bottom")};
        case K::stereo_format:
            return {p.stereo == Stereo::top_bottom ? i18n::tr("video.row.tb_format") : i18n::tr("video.row.sbs_format"),
                    (p.stereo == Stereo::top_bottom ? p.tb_format : p.sbs_format) == StereoFormat::full ? i18n::tr("video.value.full") : i18n::tr("video.value.half")};
        case K::eye_order:
            return {i18n::tr("video.row.eye_order"), p.swap_eyes ? i18n::tr("video.value.reversed") : i18n::tr("video.value.normal")};
        case K::center:
            value << std::fixed << std::setprecision(3) << p.lens.centers.at(field.index).at(field.axis);
            return {i18n::tr("video.row.image_center", {{"index", std::to_string(field.index + 1)}, {"axis", field.axis == 0 ? "X" : "Y"}}), value.str()};
        case K::distortion:
            value << std::fixed << std::setprecision(4) << p.lens.distortion.at(field.index);
            return {i18n::tr("video.row.lens", {{"index", std::to_string(field.index + 1)}}), value.str()};
        case K::reset:
            return {i18n::tr("video.row.reset"), i18n::tr("video.row.reset.detail")};
    }
    throw std::invalid_argument("Invalid settings field");
}
void adjust_setting(VideoPresentation& p, SettingField field, int direction) {
    if(direction == 0)
        return;
    using K = SettingKind;
    const auto cycle = [direction](int value, int count) { return ((value + direction) % count + count) % count; };
    switch(field.kind) {
        case K::projection:
            p.projection = static_cast<Projection>(cycle(static_cast<int>(p.projection), 4));
            if(p.projection == Projection::fisheye)
                p.fisheye_degrees = 190;
            break;
        case K::format:
            if(p.projection == Projection::fisheye)
                p.fisheye_degrees = static_cast<unsigned>(std::clamp(static_cast<long long>(p.fisheye_degrees) + direction, 1LL, 359LL));
            else
                p.projection = p.projection == Projection::hemisphere ? Projection::sphere : Projection::hemisphere;
            break;
        case K::stereo:
            p.stereo = static_cast<Stereo>(cycle(static_cast<int>(p.stereo), 3));
            break;
        case K::stereo_format: {
            auto& format = p.stereo == Stereo::top_bottom ? p.tb_format : p.sbs_format;
            format = format == StereoFormat::full ? StereoFormat::half : StereoFormat::full;
            break;
        }
        case K::eye_order:
            p.swap_eyes = !p.swap_eyes;
            break;
        case K::center: {
            auto& value = p.lens.centers.at(field.index).at(field.axis);
            value = std::clamp(value + direction * 0.001F, 0.0F, 1.0F);
            break;
        }
        case K::distortion: {
            auto& value = p.lens.distortion.at(field.index);
            value = std::clamp(value + direction * 0.0001F, -1.0F, 1.0F);
            break;
        }
        case K::reset:
            p = VideoPresentation{};
            break;
    }
}
void navigate_settings(VideoPresentation& p, std::size_t& selected, int vertical, int horizontal) {
    auto fields = settings_fields(p);
    selected = static_cast<std::size_t>(std::clamp(static_cast<long long>(std::min(selected, fields.size() - 1)) + vertical, 0LL, static_cast<long long>(fields.size() - 1)));
    const auto field = fields[selected];
    adjust_setting(p, field, horizontal);
    fields = settings_fields(p);
    selected = static_cast<std::size_t>(std::find(fields.begin(), fields.end(), field) - fields.begin());
}
}  // namespace lumaframevr::ui
