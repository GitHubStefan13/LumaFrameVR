#include "video_hints.hpp"
#include <algorithm>
#include <cctype>
#include <string_view>

namespace lumaframevr {
VideoHints VideoHints::combine(const VideoHints& other) const {
    auto combined_projection = projection.combine(other.projection);
    const auto combined_fov = fisheye_degrees.combine(other.fisheye_degrees);
    if(combined_fov.conflict && combined_projection.value == Projection::fisheye)
        combined_projection = Hint<Projection>::conflicting();
    return {combined_projection, combined_fov, stereo.combine(other.stereo), sbs_format.combine(other.sbs_format), tb_format.combine(other.tb_format), swap_eyes.combine(other.swap_eyes)};
}
VideoPresentation VideoHints::apply(VideoPresentation fallback) const {
    fallback.projection = projection.apply(fallback.projection);
    fallback.fisheye_degrees = fisheye_degrees.apply(fallback.fisheye_degrees);
    fallback.stereo = stereo.apply(fallback.stereo);
    fallback.sbs_format = sbs_format.apply(fallback.sbs_format);
    fallback.tb_format = tb_format.apply(fallback.tb_format);
    fallback.swap_eyes = swap_eyes.apply(fallback.swap_eyes);
    return fallback;
}
bool VideoHints::immersive() const {
    return projection.conflict || stereo.conflict || (projection.value && projection.value != Projection::flat) || (stereo.value && stereo.value != Stereo::mono);
}
VideoHints VideoHints::filename(const std::filesystem::path& path) {
    auto name = path.stem().native();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    VideoHints result;
    std::string previous;
    std::size_t start = 0;
    while(start < name.size()) {
        auto end = start;
        while(end < name.size() && std::isalnum(static_cast<unsigned char>(name[end]))) ++end;
        const auto token = name.substr(start, end - start);
        VideoHints hints;
        if(token == "180" || token == "vr180")
            hints.projection = Projection::hemisphere;
        else if(token == "360" || token == "vr360")
            hints.projection = Projection::sphere;
        else if(token == "fisheye" || token == "fisheye190") {
            hints.projection = Projection::fisheye;
            hints.fisheye_degrees = token == "fisheye190" ? 190U : 180U;
        } else if(token == "f180" || token == "180f" || token == "mkx200" || token == "mkx22" || token == "rf52" || token == "vrca220" || token == "eac360" || token == "360eac")
            hints.projection = Hint<Projection>::conflicting();
        if(token == "sbs" || token == "halfsbs" || token == "lr" || token == "rl") {
            hints.stereo = Stereo::side_by_side;
            hints.swap_eyes = token == "rl";
        } else if(token == "tb" || token == "halftb" || token == "bt") {
            hints.stereo = Stereo::top_bottom;
            hints.swap_eyes = token == "bt";
        } else if(token == "2d") {
            hints.stereo = Stereo::mono;
            hints.swap_eyes = false;
        }
        if(token == "halfsbs" || (previous == "half" && token == "sbs"))
            hints.sbs_format = StereoFormat::half;
        if(token == "halftb" || (previous == "half" && token == "tb"))
            hints.tb_format = StereoFormat::half;
        result = result.combine(hints);
        previous = token;
        start = end + 1;
    }
    return result;
}
VideoHints VideoHints::native(const std::array<int, 3>& fields) {
    VideoHints result;
    if(fields[0] != -1)
        result.projection = fields[0] >= 0 && fields[0] <= 2 ? Hint(static_cast<Projection>(fields[0])) : Hint<Projection>::conflicting();
    if(fields[1] != -1)
        result.stereo = fields[1] >= 0 && fields[1] <= 2 ? Hint(static_cast<Stereo>(fields[1])) : Hint<Stereo>::conflicting();
    if(fields[2] != -1)
        result.swap_eyes = fields[2] >= 0 && fields[2] <= 1 ? Hint(fields[2] != 0) : Hint<bool>::conflicting();
    return result;
}
}  // namespace lumaframevr
