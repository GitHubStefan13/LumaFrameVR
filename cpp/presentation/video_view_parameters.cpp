#include "video_view_parameters.hpp"
#include <cmath>
#include <stdexcept>

namespace lumaframevr {
VideoViewParameters view_parameters(int width, int height, float sample_aspect_ratio, const Pose& view_pose, const std::array<float, 4>& fov_angles, int eye, const VideoPresentation& presentation,
                                    float recenter) {
    if(width <= 0 || height <= 0 || eye < 0 || eye > 1)
        throw std::invalid_argument("Invalid stereo view geometry");
    VideoViewParameters result{};
    for(std::size_t i = 0; i < result.fov.size(); ++i) result.fov[i] = std::tan(fov_angles[i]);
    const auto& alignment = presentation.alignment;
    const bool flat = presentation.projection == Projection::flat;
    const float offset = presentation.stereo == Stereo::mono ? 0 : eye - 0.5F;
    result.rotation = alignment.rotation(view_pose.rotation, recenter);
    result.framing = {alignment.position[0], alignment.position[1], std::exp2(-alignment.zoom_log2), 0};
    result.stereo_offset = {alignment.stereo_offset[0] * offset, alignment.stereo_offset[1] * offset, 0, 0};
    result.aspect = static_cast<float>(width) / height * (flat ? sample_aspect_ratio : 1);
    if(presentation.stereo == Stereo::side_by_side && (!flat || presentation.sbs_format == StereoFormat::full))
        result.aspect *= 0.5F;
    if(presentation.stereo == Stereo::top_bottom && (!flat || presentation.tb_format == StereoFormat::full))
        result.aspect *= 2;
    result.projection = static_cast<std::int32_t>(presentation.projection);
    result.stereo = static_cast<std::int32_t>(presentation.stereo);
    result.eye = eye ^ static_cast<int>(presentation.swap_eyes);
    if(presentation.projection == Projection::fisheye)
        result.lens = presentation.lens.parameters(presentation.fisheye_degrees, presentation.stereo == Stereo::mono ? 0 : static_cast<std::size_t>(result.eye));
    result.distortion = presentation.lens.distortion;
    return result;
}
}  // namespace lumaframevr
