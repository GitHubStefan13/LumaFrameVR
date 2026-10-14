#pragma once
#include "decoder_session.hpp"
#include "metrics.hpp"
#include "advanced_panels.hpp"
namespace lumaframevr::diagnostics {
struct SyncTelemetry {
    double drift_ms{}, worst_ms{};
    std::uint64_t late{}, dropped{};
    std::size_t video_queue{};
    void observe(double drift, std::uint64_t dropped_frames);
    void reset_window();
    std::string verdict() const;
};
struct StreamTelemetry {
    std::uint64_t buffered_bytes{}, underruns{}, rebuffers{};
    double buffer_ms{}, read_mbps{};
    bool filling{};
    void observe(std::uint64_t bytes, double milliseconds, std::uint64_t packet_bytes, std::uint64_t underflows, std::uint64_t rejected, double seconds);
    std::string label(bool remote) const;
};
struct Telemetry {
    DecoderDescription decoder;
    SyncTelemetry sync;
    StreamTelemetry stream;
    bool remote{};
    double pts{}, bitrate_mbps{};
    std::vector<std::pair<std::string, std::string>> rows(double xr_fps, double target_fps, std::uint64_t skipped) const;
};
std::string decoder_label(const DecoderDescription& decoder);
std::string decoder_status(const DecoderDescription& decoder);
std::vector<ui::PanelRow> draw_information(ui::Canvas& canvas, const std::filesystem::path& path, const Telemetry& telemetry, const DeviceSample& device, double elapsed, double xr_fps,
                                           double target_fps, std::uint64_t skipped, std::string_view playback_status, double position, std::optional<double> duration);
std::string frame_diagnostic(const RetainedFrame& frame, const DecoderDescription& decoder);
}  // namespace lumaframevr::diagnostics
