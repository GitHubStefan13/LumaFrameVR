#include "profile_csv.hpp"
#include <chrono>
#include <cmath>
#include <ctime>
#include <stdexcept>
#include <thread>
namespace lumaframevr::diagnostics {
double process_cpu_seconds() {
    timespec time{};
    if(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time) != 0)
        throw std::runtime_error("Read process CPU clock failed");
    return time.tv_sec + time.tv_nsec * 1e-9;
}
ProfileSample profile_decode(DecoderSession& decoder, std::optional<double> seconds) {
    if(seconds && (!std::isfinite(*seconds) || *seconds <= 0))
        throw std::invalid_argument("Profile duration must be finite and positive");
    const auto started = std::chrono::steady_clock::now();
    const auto cpu = process_cpu_seconds();
    ProfileSample sample;
    for(;;) {
        auto result = decoder.advance();
        sample.elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        if(result.status == DecodeStatus::end)
            sample.eof = true;
        if(result.status == DecodeStatus::picture) {
            const auto pts = result.frame.pts();
            if(!std::isfinite(pts))
                throw std::runtime_error("Profile decoded nonfinite timestamp");
            if(!sample.first)
                sample.first = pts;
            sample.last = pts;
            ++sample.frames;
        }
        if(sample.eof || (seconds && sample.elapsed_ms >= *seconds * 1000))
            break;
        if(result.status == DecodeStatus::pending)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if(!seconds && sample.elapsed_ms > 30000)
            throw std::runtime_error("Profile decode timed out after 30 seconds");
    }
    if(sample.elapsed_ms > 0)
        sample.cpu_percent = (process_cpu_seconds() - cpu) * 100000 / sample.elapsed_ms;
    return sample;
}
}  // namespace lumaframevr::diagnostics
