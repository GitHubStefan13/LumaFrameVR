#pragma once
#include "pose_math.hpp"
#include <array>

namespace lumaframevr {
enum class Projection { flat, hemisphere, sphere, fisheye };
enum class Stereo { mono, side_by_side, top_bottom };
enum class StereoFormat { full, half };
enum class AlignmentMode { orientation, position };
struct VideoAlignment {
    float yaw{}, pitch{}, roll{}, zoom_log2{};
    std::array<float, 2> stereo_offset{}, position{};
    void adjust(AlignmentMode mode, const std::array<std::array<float, 2>, 2>& sticks, float seconds, Stereo stereo, float vertical_sign);
    Quaternion rotation(Quaternion head, float recenter) const;
    bool operator==(const VideoAlignment&) const = default;
};
struct VideoLens {
    std::array<std::array<float, 2>, 2> centers{{{0.5F, 0.5F}, {0.5F, 0.5F}}};
    std::array<float, 4> distortion{};
    std::array<float, 4> parameters(unsigned degrees, std::size_t eye) const;
    bool operator==(const VideoLens&) const = default;
};
struct VideoPresentation {
    Projection projection{Projection::hemisphere};
    Stereo stereo{Stereo::side_by_side};
    StereoFormat sbs_format{}, tb_format{};
    bool swap_eyes{};
    unsigned fisheye_degrees{180};
    VideoAlignment alignment;
    VideoLens lens;
    static VideoPresentation flat();
    bool operator==(const VideoPresentation&) const = default;
};
}  // namespace lumaframevr
