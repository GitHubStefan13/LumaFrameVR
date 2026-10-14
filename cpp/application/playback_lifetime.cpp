#include "playback_lifetime.hpp"
#include <stdexcept>
namespace lumaframevr::application {
PlaybackLifetime::~PlaybackLifetime() {
    player_.reset();
    retired_.reset();
    if(stopping_.valid())
        stopping_.wait();
}
void PlaybackLifetime::start(const MediaSource& source, std::optional<std::pair<float, bool>> initial_volume) {
    if(playing() || retiring())
        throw std::logic_error("playback lifetime still occupied");
    player_ = std::make_unique<PlaybackTimeline>(source, false, initial_volume);
}
void PlaybackLifetime::stop() {
    pending_.reset();
    if(player_)
        retired_ = std::move(player_);
}
void PlaybackLifetime::queue(std::filesystem::path path) {
    pending_ = std::move(path);
}
bool PlaybackLifetime::reap() {
    if(!stopping_.valid() || stopping_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return false;
    stopping_.get();
    return true;
}
std::optional<std::filesystem::path> PlaybackLifetime::take_pending() {
    if(retiring())
        return {};
    auto result = std::move(pending_);
    pending_.reset();
    return result;
}
void PlaybackLifetime::synchronize(const std::function<void()>& release_graphics) {
    synchronize(release_graphics, {});
}
void PlaybackLifetime::synchronize(const std::function<void()>& release_graphics, const std::function<void(const std::exception&)>& failure) {
    bool released = false;
    if(retired_) {
        release_graphics();
        released = true;
        stopping_ = std::async(std::launch::async, [player = std::move(retired_)]() mutable { player.reset(); });
    }
    if(player_) {
        WorkerSnapshot status;
        try {
            status = player_->snapshot();
        } catch(const std::exception& error) {
            if(!failure)
                throw;
            failure(error);
            return;
        }
        if(status.release_required) {
            release_graphics();
            player_->released(status.generation);
        }
    } else if(!released)
        release_graphics();
}
}  // namespace lumaframevr::application
