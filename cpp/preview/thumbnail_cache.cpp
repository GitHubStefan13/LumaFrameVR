#include "thumbnail_cache_state.hpp"
#include <algorithm>
#include <utility>
namespace lumaframevr {
ThumbnailCache::ThumbnailCache() : state_(std::make_unique<State>()) {}
ThumbnailCache::~ThumbnailCache() {
    std::shared_ptr<ThumbnailPersistenceOwner> owner;
    {
        std::lock_guard lock(state_->mutex);
        state_->stopped = true;
        state_->pending.clear();
        owner = state_->disk_owner;
    }
    owner->revoke();
    state_->changed.notify_all();
    if(state_->worker.joinable())
        state_->worker.join();
}
void ThumbnailCache::want(const TileWanted& wanted) {
    if(state_->wanted == wanted)
        return;
    state_->wanted = wanted;
    ++state_->tick;
    if(!state_->worker.joinable() && !wanted.entries.empty())
        state_->worker = std::thread([this] { state_->run(); });
    std::vector<State::Job> jobs;
    for(auto item = wanted.entries.rbegin(); item != wanted.entries.rend(); ++item) {
        const State::Key key{item->second, wanted.width};
        const auto cached = state_->ready.find(key);
        if(cached != state_->ready.end()) {
            cached->second.used = state_->tick;
            continue;
        }
        if(!state_->failures.contains(key))
            jobs.push_back({key, state_->reset_serial});
    }
    state_->evict();
    {
        std::lock_guard lock(state_->mutex);
        state_->pending = std::move(jobs);
    }
    state_->changed.notify_one();
}
void ThumbnailCache::update() {
    std::deque<State::Completed> results;
    {
        std::lock_guard lock(state_->mutex);
        results.swap(state_->completed);
    }
    for(auto& result : results) {
        if(result.job.reset != state_->reset_serial)
            continue;
        if(result.pixels && result.job.key.second == state_->wanted.width) {
            const bool visible = std::any_of(state_->wanted.entries.begin(), state_->wanted.entries.end(), [&](const auto& entry) { return entry.second == result.job.key.first; });
            if(visible)
                ++state_->revision;
            state_->ready.insert_or_assign(result.job.key, State::Entry{std::move(result.pixels), state_->tick});
        } else if(!result.pixels)
            state_->failures.insert_or_assign(result.job.key, std::move(result.error));
    }
    state_->evict();
}
std::vector<TileShown> ThumbnailCache::shown(const TileWanted& wanted) const {
    std::vector<TileShown> output;
    for(const auto& [index, path] : wanted.entries) {
        const auto found = state_->ready.find({path, wanted.width});
        if(found != state_->ready.end())
            output.push_back({index, found->second.pixels});
    }
    return output;
}
void ThumbnailCache::reset() {
    ++state_->revision;
    state_->ready.clear();
    state_->failures.clear();
    state_->wanted = {};
    std::shared_ptr<ThumbnailPersistenceOwner> owner;
    {
        std::lock_guard lock(state_->mutex);
        owner = std::exchange(state_->disk_owner, std::make_shared<ThumbnailPersistenceOwner>());
        ++state_->reset_serial;
        state_->reset_worker = true;
        state_->source_count = 0;
        state_->pending.clear();
        state_->completed.clear();
    }
    owner->revoke();
    state_->changed.notify_one();
}
std::size_t ThumbnailCache::held() const {
    return state_->ready.size();
}
std::size_t ThumbnailCache::failed() const {
    return state_->failures.size();
}
std::optional<std::string> ThumbnailCache::failure(const std::filesystem::path& path, unsigned width) const {
    const auto found = state_->failures.find({path, width});
    return found == state_->failures.end() ? std::nullopt : std::optional{found->second};
}
std::size_t ThumbnailCache::bytes() const {
    return state_->bytes();
}
std::size_t ThumbnailCache::sources() const {
    return state_->source_count.load();
}
std::uint64_t ThumbnailCache::revision() const {
    return state_->revision;
}
}  // namespace lumaframevr
