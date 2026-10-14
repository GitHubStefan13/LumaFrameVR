#pragma once
#include "decoder_session.hpp"
#include <filesystem>
#include <ostream>
namespace lumaframevr::diagnostics {
inline constexpr std::string_view profile_header =
        "run,mode,operation,target_s,elapsed_ms,frames,first_pts_s,last_pts_s,skipped,eof,poll_ms,import_ms,draw_ms,gpu_ms,eye_width,eye_height,preview_pts_s,preview_ms,preview_ready_ms,cpu_percent";
inline constexpr std::string_view thumbnail_header =
        "run,request,operation,target_s,visit,cache,status,elapsed_ms,preview_pts_s,preview_ms,preview_ready_ms,release_handshake_ms,worker_wait_ms,handoff_ms,audio_reset_ms,container_seek_ms,"
        "decoder_close_ms,decoder_open_ms,read_ms,send_ms,receive_ms,audio_ms,seek_calls,packets,packet_bytes,send_calls,receive_calls,eagain,decoded_frames,rejected_frames,import_ms,draw_ms,gpu_ms,"
        "submit_ms,fence_wait_ms,frames,skipped,first_pts_s,audio_clock_s,scenario";
struct ProfileSample {
    double elapsed_ms{};
    std::uint64_t frames{}, skipped{};
    std::optional<double> first, last, cpu_percent;
    bool eof{};
    std::optional<double> import_ms, draw_ms, gpu_ms;
    std::array<int, 2> eye_size{};
    std::optional<double> preview_pts, preview_ms, preview_ready_ms, audio_clock;
};
struct NativeProfileTrace {
    std::array<std::int64_t, 8> microseconds{};
    std::array<std::uint64_t, 8> counters{};
};
struct ThumbnailProfileSample {
    ProfileSample sample;
    std::optional<NativeProfileTrace> native;
    std::optional<double> release_handshake_ms, worker_wait_ms, handoff_ms, submit_ms, fence_wait_ms;
};
struct ProfileRequest {
    unsigned run{1};
    std::size_t request{};
    std::string operation, scenario;
    std::optional<double> target;
    bool repeated{};
    std::string profile_csv(const ProfileSample& sample, std::string_view mode, double poll_ms) const;
    std::string thumbnail_csv(std::string_view status, const std::optional<ThumbnailProfileSample>& sample, double elapsed_ms) const;
};
ProfileSample profile_decode(DecoderSession& decoder, std::optional<double> seconds = std::nullopt);
double process_cpu_seconds();
ThumbnailProfileSample profile_native_still(const std::filesystem::path& path, double target);
void write_profile_row(std::ostream& output, std::string_view row);
}  // namespace lumaframevr::diagnostics
