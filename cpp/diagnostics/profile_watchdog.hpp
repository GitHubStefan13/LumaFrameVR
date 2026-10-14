#pragma once
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
namespace lumaframevr::diagnostics {
class ProfileWatchdog {
   public:
    ProfileWatchdog(std::chrono::duration<double> limit, std::string stage, std::function<void()> timeout = {});
    ~ProfileWatchdog();
    ProfileWatchdog(const ProfileWatchdog&) = delete;
    ProfileWatchdog& operator=(const ProfileWatchdog&) = delete;

   private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool cancelled_{};
    std::thread thread_;
};
}  // namespace lumaframevr::diagnostics
