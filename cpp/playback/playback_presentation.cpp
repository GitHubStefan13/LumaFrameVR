#include "playback_timeline.hpp"
#include <algorithm>

namespace lumaframevr {
RetainedFrame PlaybackTimeline::poll() {
    auto status = worker_.snapshot();
    length_ = status.duration;
    if(pause_started_ && !target_)
        return {};
    RetainedFrame latest;
    for(;;) {
        if(!pending_.valid() && !ended_) {
            auto decoded = worker_.next();
            if(decoded.status == DecodeStatus::picture)
                pending_ = std::move(decoded.frame);
            else if(decoded.status == DecodeStatus::preview) {
                preview_ = std::move(decoded.frame);
                break;
            } else if(decoded.status == DecodeStatus::end) {
                ended_ = true;
                if(target_) {
                    position_ = std::min(*target_, length_.value_or(*target_));
                    target_.reset();
                }
            } else
                break;
        }
        if(!pending_.valid())
            break;
        const bool first = !started_;
        if(first && !worker_.snapshot().ready)
            break;
        auto now = std::chrono::steady_clock::now();
        if(first) {
            started_ = now;
            first_pts_ = pending_.pts();
            paused_time_ = {};
            target_.reset();
            if(pause_started_)
                pause_started_ = now;
            worker_.begin();
        }
        if(auto clock = worker_.audio_clock()) {
            first_pts_ = *clock;
            started_ = now;
            paused_time_ = {};
        }
        const auto time = std::chrono::duration<double>(std::max(now - *started_ - paused_time_, std::chrono::steady_clock::duration{})).count();
        if(!first && pending_.pts() - first_pts_ > time)
            break;
        if(latest.valid())
            ++skipped_;
        latest = std::move(pending_);
        if(pause_started_)
            break;
    }
    if(latest.valid())
        position_ = std::max(0.0, latest.pts());
    return latest;
}
}  // namespace lumaframevr
