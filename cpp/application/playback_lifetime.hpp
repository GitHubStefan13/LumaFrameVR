#pragma once
#include "playback_timeline.hpp"
#include <functional>
#include <filesystem>
#include <future>
#include <optional>
#include <utility>
namespace lumaframevr::application {
class PlaybackLifetime {
   public:
    ~PlaybackLifetime();
    void start(const MediaSource& source, std::optional<std::pair<float, bool>> initial_volume = {});
    void stop();
    void queue(std::filesystem::path path);
    bool reap();
    std::optional<std::filesystem::path> take_pending();
    void synchronize(const std::function<void()>& release_graphics);
    void synchronize(const std::function<void()>& release_graphics, const std::function<void(const std::exception&)>& failure);
    PlaybackTimeline* current() const { return player_.get(); }
    bool playing() const { return player_ != nullptr; }
    bool retiring() const { return retired_ != nullptr || stopping_.valid(); }
    const std::optional<std::filesystem::path>& queued() const { return pending_; }

   private:
    std::unique_ptr<PlaybackTimeline> player_, retired_;
    std::future<void> stopping_;
    std::optional<std::filesystem::path> pending_;
};
}  // namespace lumaframevr::application
