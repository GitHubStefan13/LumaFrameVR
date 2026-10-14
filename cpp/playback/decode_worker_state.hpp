#pragma once
#include "decode_worker.hpp"
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>

namespace lumaframevr {
struct DecodeWorker::Shared {
    std::mutex mutex;
    std::condition_variable changed;
    WorkerSnapshot status;
    std::deque<DecodeResult> frames;
    std::optional<double> seek;
    std::optional<std::pair<float, bool>> volume;
    std::optional<std::chrono::steady_clock::time_point> audio_observed;
    std::exception_ptr error;
    bool stopped{}, running{}, preview{};
};
}  // namespace lumaframevr
