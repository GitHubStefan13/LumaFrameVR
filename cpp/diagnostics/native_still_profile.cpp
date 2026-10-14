#include "profile_csv.hpp"
#include "media_session.hpp"
#include "media_owned_frame.hpp"
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <thread>
namespace lumaframevr::diagnostics {
ThumbnailProfileSample profile_native_still(const std::filesystem::path& path, double target) {
    if(!std::isfinite(target) || target < 0)
        throw std::invalid_argument("Invalid native profile target");
    const auto started = std::chrono::steady_clock::now();
    const auto cpu = process_cpu_seconds();
    auto opened = media::open(path.c_str(), true);
    if(const auto* error = std::get_if<media::Error>(&opened))
        throw std::runtime_error("Open native profiling decoder: " + error->message);
    auto decoder = std::move(std::get<media::Session>(opened));
    decoder.profile(true);
    const auto seek = decoder.seek(target);
    if(seek.status < 0)
        throw std::runtime_error("Seek native profiling decoder: " + seek.error);
    media::Frame frame;
    for(;;) {
        const auto result = decoder.next(frame);
        if(result.status < 0)
            throw std::runtime_error("Decode native profiling still: " + result.error);
        if(frame.get().owner)
            break;
        if(result.status == 0)
            throw std::runtime_error("Native profiling source has no still");
        if(std::chrono::steady_clock::now() - started > std::chrono::seconds(30))
            throw std::runtime_error("Native profiling still timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto trace = decoder.trace();
    ThumbnailProfileSample sample;
    sample.sample.elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    sample.sample.preview_pts = frame.get().pts;
    sample.sample.preview_ms = sample.sample.elapsed_ms;
    sample.sample.frames = 1;
    sample.sample.first = frame.get().pts;
    sample.sample.cpu_percent = (process_cpu_seconds() - cpu) * 100000 / sample.sample.elapsed_ms;
    sample.native = NativeProfileTrace{{trace.audio_reset_us, trace.seek_us, trace.close_us, trace.open_us, trace.read_us, trace.send_us, trace.receive_us, trace.audio_us},
                                       {trace.seek_calls, trace.packets, trace.packet_bytes, trace.send_calls, trace.receive_calls, trace.again, trace.frames, trace.rejected}};
    return sample;
}
}  // namespace lumaframevr::diagnostics
