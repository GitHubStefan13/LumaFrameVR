#include "advanced_panels.hpp"
#include "catalogue.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace lumaframevr::ui {
namespace {
std::string numeric(float value, int decimals, bool sign) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(decimals);
    if(sign)
        output << std::showpos;
    output << value;
    return output.str();
}
}  // namespace
std::array<AlignmentSlider, 4> alignment_sliders(const VideoPresentation& p, AlignmentMode mode) {
    const auto& a = p.alignment;
    const auto fraction = [](float value, float limit) { return std::clamp((value + limit) / (limit * 2), 0.0F, 1.0F); };
    if(mode == AlignmentMode::orientation)
        return {{{i18n::tr("rotation.yaw"), i18n::tr("rotation.degrees", {{"value", numeric(a.yaw, 1, true)}}), fraction(a.yaw, 180), true},
                 {i18n::tr("rotation.pitch"), i18n::tr("rotation.degrees", {{"value", numeric(a.pitch, 1, true)}}), fraction(a.pitch, 90), true},
                 {i18n::tr("rotation.roll"), i18n::tr("rotation.degrees", {{"value", numeric(a.roll, 1, true)}}), fraction(a.roll, 180), true},
                 {i18n::tr("rotation.zoom"), i18n::tr("rotation.percent", {{"value", numeric(std::exp2(a.zoom_log2) * 100, 0, false)}}), fraction(a.zoom_log2, 2), true}}};
    const bool enabled = p.stereo != Stereo::mono;
    return {{{i18n::tr("rotation.stereo_offset_h"), enabled ? i18n::tr("rotation.percent", {{"value", numeric(a.stereo_offset[0] * 100, 2, true)}}) : i18n::tr("rotation.unavailable_mono"),
              fraction(a.stereo_offset[0], 0.1F), enabled},
             {i18n::tr("rotation.stereo_offset_v"), enabled ? i18n::tr("rotation.percent", {{"value", numeric(a.stereo_offset[1] * 100, 2, true)}}) : i18n::tr("rotation.unavailable_mono"),
              fraction(a.stereo_offset[1], 0.1F), enabled},
             {i18n::tr("rotation.position_h"), numeric(a.position[0], 2, true), fraction(a.position[0], 0.5F), true},
             {i18n::tr("rotation.position_v"), numeric(a.position[1], 2, true), fraction(a.position[1], 0.5F), true}}};
}
void set_alignment_slider(VideoPresentation& p, AlignmentMode mode, std::size_t row, float fraction) {
    if(row >= 4 || !std::isfinite(fraction))
        return;
    const auto value = [fraction](float limit) { return -limit + std::clamp(fraction, 0.0F, 1.0F) * limit * 2; };
    auto& a = p.alignment;
    if(mode == AlignmentMode::orientation) {
        switch(row) {
            case 0:
                a.yaw = value(180);
                break;
            case 1:
                a.pitch = value(90);
                break;
            case 2:
                a.roll = value(180);
                break;
            case 3:
                a.zoom_log2 = value(2);
                break;
        }
    } else if(row < 2) {
        if(p.stereo != Stereo::mono)
            a.stereo_offset[row] = value(0.1F);
    } else
        a.position[row - 2] = value(0.5F);
}
void step_alignment(VideoPresentation& p, AlignmentMode mode, std::size_t row, int direction) {
    if(direction == 0) {
        p.alignment = {};
        return;
    }
    const float delta = direction > 0 ? 1.0F : -1.0F;
    auto& a = p.alignment;
    if(mode == AlignmentMode::orientation) {
        const auto wrap = [](float value) { return value - 360 * std::floor((value + 180) / 360); };
        switch(row) {
            case 0:
                a.yaw = wrap(a.yaw + delta);
                break;
            case 1:
                a.pitch = std::clamp(a.pitch + delta, -90.0F, 90.0F);
                break;
            case 2:
                a.roll = wrap(a.roll + delta);
                break;
            case 3:
                a.zoom_log2 = std::clamp(a.zoom_log2 + delta * 0.05F, -2.0F, 2.0F);
                break;
        }
    } else if(row < 2) {
        if(p.stereo != Stereo::mono)
            a.stereo_offset[row] = std::clamp(a.stereo_offset[row] + delta * 0.001F, -0.1F, 0.1F);
    } else if(row < 4)
        a.position[row - 2] = std::clamp(a.position[row - 2] + delta * 0.01F, -0.5F, 0.5F);
}
}  // namespace lumaframevr::ui
