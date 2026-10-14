#pragma once
#include "video_presentation.hpp"
#include <cstdint>

namespace lumaframevr {
struct VideoViewParameters {
    std::array<float, 4> fov, rotation, framing, stereo_offset;
    float aspect;
    std::int32_t projection, stereo, eye;
    std::array<float, 4> lens, distortion;
};
VideoViewParameters view_parameters(int width, int height, float sample_aspect_ratio, const Pose& view_pose, const std::array<float, 4>& fov_angles, int eye, const VideoPresentation& presentation,
                                    float recenter);
}  // namespace lumaframevr
