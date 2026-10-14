#include "decoder_internal.hpp"
#include <algorithm>
#include <cmath>

namespace lumaframevr {
void DecoderSession::seek(double seconds) {
    if(!std::isfinite(seconds) || seconds < 0)
        throw std::invalid_argument("Invalid seek target");
    const auto result = session_->native.seek(seconds);
    check_native(result.status, result.error.c_str());
}
void DecoderSession::preview() {
    const auto result = session_->native.preview();
    check_native(result.status, result.error.c_str());
}
bool DecoderSession::reconfigure(bool released) {
    const auto result = session_->native.reconfigure(released);
    check_native(result.status, result.error.c_str());
    return result.status != 0;
}
void DecoderSession::enable_audio() {
    const auto result = session_->native.enable_audio();
    check_native(result.status, result.error.c_str());
}
AudioObservation DecoderSession::audio(bool paused) {
    const auto tick = session_->native.audio(paused);
    check_native(tick.result.status, tick.result.error.c_str());
    return {tick.clock >= 0 ? std::optional(tick.clock) : std::nullopt, tick.limit, tick.done != 0, tick.result.status > 0};
}
void DecoderSession::volume(float level, bool muted) {
    if(!std::isfinite(level))
        throw std::invalid_argument("Invalid audio volume");
    if(!session_->native.has_audio())
        return;
    const auto result = session_->native.volume(std::clamp(level, 0.0F, 1.0F), muted);
    check_native(result.status, result.error.c_str());
}
std::optional<double> AudioObservation::position(double elapsed) const {
    if(!clock || done)
        return std::nullopt;
    return std::min(*clock + std::max(0.0, elapsed), limit);
}
}  // namespace lumaframevr
