#include "playback_timeline.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace lumaframevr {
PlaybackTimeline::PlaybackTimeline(MediaSource source, bool software, std::optional<std::pair<float, bool>> initial_volume) : worker_(std::move(source), software, initial_volume) {}
std::uint64_t PlaybackTimeline::seek(double target) {
    if(!std::isfinite(target) || target < 0)
        throw std::invalid_argument("Invalid seek target");
    length_ = worker_.snapshot().duration;
    pending_ = {};
    preview_ = {};
    target_ = std::min(target, length_.value_or(target));
    started_.reset();
    ended_ = false;
    return worker_.seek(*target_);
}
std::optional<std::uint64_t> PlaybackTimeline::skip(int seconds) {
    length_ = worker_.snapshot().duration;
    const auto end = length_.value_or(std::numeric_limits<double>::infinity());
    const auto position = target_.value_or(position_);
    const auto target = std::clamp(std::min(position, end) + seconds, 0.0, end);
    return target == position ? std::nullopt : std::optional(seek(target));
}
RetainedFrame PlaybackTimeline::take_preview() {
    return std::move(preview_);
}
void PlaybackTimeline::pause(bool paused) {
    manual_pause_ = paused;
    update_pause();
}
void PlaybackTimeline::suspend(bool suspended) {
    suspended_ = suspended;
    update_pause();
}
void PlaybackTimeline::update_pause() {
    const auto now = std::chrono::steady_clock::now();
    if((manual_pause_ || suspended_) && !pause_started_)
        pause_started_ = now;
    else if(!manual_pause_ && !suspended_ && pause_started_) {
        paused_time_ += now - *pause_started_;
        pause_started_.reset();
    }
    worker_.pause(pause_started_.has_value());
}
}  // namespace lumaframevr
