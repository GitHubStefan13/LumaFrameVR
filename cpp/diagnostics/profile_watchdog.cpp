#include "profile_watchdog.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
namespace lumaframevr::diagnostics {
ProfileWatchdog::ProfileWatchdog(std::chrono::duration<double> limit, std::string stage, std::function<void()> timeout) {
    if(!std::isfinite(limit.count()) || limit.count() <= 0)
        throw std::invalid_argument("Invalid profiling watchdog duration");
    thread_ = std::thread([this, limit, stage = std::move(stage), timeout = std::move(timeout)] {
        const auto now = std::chrono::steady_clock::now();
        const auto remaining = std::chrono::duration<double>(std::chrono::steady_clock::time_point::max() - now);
        const auto deadline = limit >= remaining ? std::chrono::steady_clock::time_point::max() : now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(limit);
        std::unique_lock lock(mutex_);
        if(condition_.wait_until(lock, deadline, [this] { return cancelled_; }))
            return;
        lock.unlock();
        try {
            if(timeout)
                timeout();
        } catch(...) {
        }
        std::cerr << "LumaFrameVR profile: " << stage << " timed out after " << limit.count() << "s\n";
        std::cout.flush();
        std::cerr.flush();
        std::fflush(nullptr);
        std::_Exit(124);
    });
}
ProfileWatchdog::~ProfileWatchdog() {
    {
        std::lock_guard lock(mutex_);
        cancelled_ = true;
    }
    condition_.notify_all();
    if(thread_.joinable())
        thread_.join();
}
}  // namespace lumaframevr::diagnostics
