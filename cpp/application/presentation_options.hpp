#pragma once
#include "video_hints.hpp"
#include <string_view>
namespace lumaframevr {
StereoFormat parse_stereo_format(std::string_view value);
void parse_projection(VideoHints& hints, std::string_view value);
void parse_stereo(VideoHints& hints, std::string_view value);
}  // namespace lumaframevr
