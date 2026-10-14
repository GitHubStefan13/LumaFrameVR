#include "video_presentation.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace lumaframevr {
namespace {
float response(float value) {
    return std::copysign(std::pow(std::clamp((std::abs(value) - 0.15F) / 0.85F, 0.0F, 1.0F), 2), value);
}
float wrap(float degrees) {
    return degrees - 360 * std::floor((degrees + 180) / 360);
}
}  // namespace
VideoPresentation VideoPresentation::flat() {
    VideoPresentation presentation;
    presentation.projection = Projection::flat;
    presentation.stereo = Stereo::mono;
    return presentation;
}
void VideoAlignment::adjust(AlignmentMode mode, const std::array<std::array<float, 2>, 2>& sticks, float seconds, Stereo stereo, float vertical_sign) {
    const auto x = response(sticks[0][0]) * seconds;
    const auto y = response(sticks[0][1]) * seconds * vertical_sign;
    const auto rx = response(sticks[1][0]) * seconds;
    const auto ry = response(sticks[1][1]) * seconds * vertical_sign;
    if(mode == AlignmentMode::orientation) {
        yaw = wrap(yaw - x * 45);
        pitch = std::clamp(pitch - y * 45, -90.0F, 90.0F);
        roll = wrap(roll - rx * 45);
        zoom_log2 = std::clamp(zoom_log2 + ry * 0.75F, -2.0F, 2.0F);
    } else {
        if(stereo != Stereo::mono) {
            stereo_offset[0] = std::clamp(stereo_offset[0] + x * 0.02F, -0.1F, 0.1F);
            stereo_offset[1] = std::clamp(stereo_offset[1] + y * 0.02F, -0.1F, 0.1F);
        }
        position[0] = std::clamp(position[0] - rx * 0.4F, -0.5F, 0.5F);
        position[1] = std::clamp(position[1] - ry * 0.4F, -0.5F, 0.5F);
    }
}
Quaternion VideoAlignment::rotation(Quaternion head, float recenter) const {
    const float radians = std::numbers::pi_v<float> / 180;
    const std::array turns{std::pair{1, yaw * radians - recenter}, std::pair{0, -pitch * radians}, std::pair{2, roll * radians}};
    for(auto [axis, angle] : turns) {
        Quaternion turn{0, 0, 0, std::cos(angle / 2)};
        turn[axis] = std::sin(angle / 2);
        head = multiply(turn, head);
    }
    return head;
}
std::array<float, 4> VideoLens::parameters(unsigned degrees, std::size_t eye) const {
    if(eye >= centers.size())
        throw std::out_of_range("Fisheye eye");
    const float angle = degrees * std::numbers::pi_v<float> / 360;
    const float squared = angle * angle;
    const auto [k1, k2, k3, k4] = distortion;
    const float radius = angle * (1 + squared * (k1 + squared * (k2 + squared * (k3 + squared * k4))));
    return {centers[eye][0], centers[eye][1], angle, radius};
}
}  // namespace lumaframevr
