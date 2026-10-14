#include "advanced_panels.hpp"
#include <algorithm>
#include <cmath>
namespace lumaframevr::ui {
std::optional<float> setting_fraction(const VideoPresentation& p, SettingField field) {
    switch(field.kind) {
        case SettingKind::format:
            if(p.projection == Projection::fisheye)
                return (static_cast<float>(p.fisheye_degrees) - 1) / 358;
            return std::nullopt;
        case SettingKind::center:
            return p.lens.centers.at(field.index).at(field.axis);
        case SettingKind::distortion:
            return (p.lens.distortion.at(field.index) + 1) / 2;
        default:
            return std::nullopt;
    }
}
void set_setting_fraction(VideoPresentation& p, SettingField field, float fraction) {
    fraction = std::clamp(fraction, 0.0F, 1.0F);
    switch(field.kind) {
        case SettingKind::format:
            if(p.projection == Projection::fisheye)
                p.fisheye_degrees = static_cast<unsigned>(std::lround(1 + 358 * fraction));
            break;
        case SettingKind::center:
            p.lens.centers.at(field.index).at(field.axis) = fraction;
            break;
        case SettingKind::distortion:
            p.lens.distortion.at(field.index) = fraction * 2 - 1;
            break;
        default:
            break;
    }
}
}  // namespace lumaframevr::ui
