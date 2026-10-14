#include "thumbnail_services.hpp"
#include "transport.hpp"
#include <algorithm>
#include <cmath>
namespace lumaframevr {
bool PreviewCadence::due(double now) {
    if(!std::isfinite(now))
        throw std::invalid_argument("Nonfinite preview cadence time");
    if(previous_ && now - *previous_ < 1.5)
        return false;
    previous_ = now;
    return true;
}
bool SeekPreviewReadout::arm(std::uint64_t request) {
    if(request_ == request)
        return false;
    request_ = request;
    armed_ = true;
    shown_.reset();
    return true;
}
bool SeekPreviewReadout::should_paint(bool frame) const {
    return armed_ && !shown_ && frame;
}
void SeekPreviewReadout::painted(double pts) {
    if(armed_)
        shown_ = pts;
}
void SeekPreviewReadout::visibility(bool transport_visible) {
    if(!transport_visible) {
        armed_ = false;
        shown_.reset();
    }
}
float SeekPreviewReadout::center(double position, std::optional<double> duration) {
    const auto track = ui::transport_track(1160);
    float marker = track.x + track.width / 2.0F;
    if(duration && *duration > 0 && std::isfinite(position))
        marker = track.x + static_cast<float>(std::round(std::clamp(position / *duration, 0.0, 1.0) * (track.width - 1)));
    return (std::clamp(marker, track.x + 128.0F, track.right() - 128.0F) / 1160 - 0.5F) * (1.6F * (0.6F / 1.4F) * 1.8F);
}
}  // namespace lumaframevr
