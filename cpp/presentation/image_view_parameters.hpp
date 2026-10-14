#pragma once
#include "video_view_parameters.hpp"
namespace lumaframevr {
inline VideoViewParameters image_view_parameters(int width, int height, const Pose& pose, const std::array<float, 4>& fov, int eye, const VideoPresentation& presentation, float recenter) {
    auto result = view_parameters(width, height, 1, pose, fov, eye, presentation, recenter);
    result.framing[3] = -1;
    return result;
}
}  // namespace lumaframevr
