#pragma once
#include "decode_worker.hpp"
#include <optional>
#include <utility>

namespace lumaframevr {
class PlaybackTimeline {
   public:
    explicit PlaybackTimeline(MediaSource source, bool software = false, std::optional<std::pair<float, bool>> initial_volume = {});
    RetainedFrame poll();
    RetainedFrame take_preview();
    std::uint64_t seek(double target);
    std::optional<std::uint64_t> skip(int seconds);
    void released(std::uint64_t generation) { worker_.released(generation); }
    bool release_required() const { return worker_.snapshot().release_required; }
    void pause(bool paused);
    void suspend(bool suspended);
    void request_preview() { worker_.preview(); }
    void volume(float level, bool muted) { worker_.volume(level, muted); }
    bool finished() const { return ended_ && !pending_.valid(); }
    double position() const { return position_; }
    std::optional<double> duration() const { return length_; }
    WorkerSnapshot snapshot() const { return worker_.snapshot(); }

   private:
    void update_pause();
    DecodeWorker worker_;
    RetainedFrame pending_, preview_;
    std::optional<std::chrono::steady_clock::time_point> started_, pause_started_;
    std::chrono::steady_clock::duration paused_time_{};
    bool manual_pause_{}, suspended_{}, ended_{};
    double first_pts_{}, position_{};
    std::optional<double> target_, length_;
    std::uint64_t skipped_{};
};
}  // namespace lumaframevr
