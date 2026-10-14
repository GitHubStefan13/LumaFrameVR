#include "thumbnail_cache_state.hpp"
#include <utility>
namespace lumaframevr {
void ThumbnailCache::persistence(std::optional<std::filesystem::path> directory, std::size_t budget) {
    std::shared_ptr<ThumbnailPersistenceOwner> owner;
    {
        std::lock_guard lock(state_->mutex);
        if(state_->disk_directory == directory && state_->disk_budget == budget)
            return;
        owner = std::exchange(state_->disk_owner, std::make_shared<ThumbnailPersistenceOwner>());
        state_->disk_directory = std::move(directory);
        state_->disk_budget = budget;
        ++state_->disk_serial;
        state_->disk_error.clear();
    }
    owner->revoke();
    state_->changed.notify_one();
}
std::uint64_t ThumbnailCache::disk_hits() const {
    return state_->disk_hits.load();
}
std::string ThumbnailCache::persistence_error() const {
    std::lock_guard lock(state_->mutex);
    return state_->disk_error;
}
}  // namespace lumaframevr
