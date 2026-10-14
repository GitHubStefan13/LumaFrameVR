#pragma once
#include <functional>
#include <mutex>
namespace lumaframevr {
class ThumbnailPersistenceOwner {
   public:
    bool publish(const std::function<void()>& commit) {
        std::lock_guard lock(mutex_);
        if(!active_)
            return false;
        commit();
        return true;
    }
    void revoke() {
        std::lock_guard lock(mutex_);
        active_ = false;
    }

   private:
    std::mutex mutex_;
    bool active_{true};
};
}  // namespace lumaframevr
