#include "telemetry.hpp"
#include "catalogue.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>
namespace lumaframevr::diagnostics {
std::string decoder_label(const DecoderDescription& d) {
    if(d.name.empty())
        return i18n::tr("telemetry.not_opened");
    std::ostringstream out;
    out << d.name << ' ' << d.width << 'x' << d.height << ' ' << d.bits << "-bit " << std::fixed << std::setprecision(1) << d.fps << "fps";
    return out.str();
}
std::string decoder_status(const DecoderDescription& d) {
    return i18n::tr(d.name.empty() ? "telemetry.pending" : d.hardware ? "telemetry.hardware" : "telemetry.software");
}
void SyncTelemetry::observe(double drift, std::uint64_t dropped_frames) {
    drift_ms = drift;
    if(std::abs(drift) > std::abs(worst_ms))
        worst_ms = drift;
    dropped = dropped_frames;
}
void SyncTelemetry::reset_window() {
    worst_ms = drift_ms;
    late = 0;
}
std::string SyncTelemetry::verdict() const {
    return i18n::tr(std::abs(drift_ms) < 20 ? "telemetry.sync_in" : std::abs(drift_ms) < 80 ? "telemetry.sync_slight" : "telemetry.sync_out");
}
void StreamTelemetry::observe(std::uint64_t bytes, double milliseconds, std::uint64_t packet_bytes, std::uint64_t underflows, std::uint64_t rejected, double seconds) {
    buffered_bytes = bytes;
    buffer_ms = milliseconds;
    underruns = underflows;
    rebuffers = rejected;
    if(seconds > 0)
        read_mbps = packet_bytes * 8.0 / seconds / 1e6;
}
std::string StreamTelemetry::label(bool remote) const {
    const auto integer = [](double value) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(0) << value;
        return out.str();
    };
    const auto decimal = [](double value) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(1) << value;
        return out.str();
    };
    if(!remote)
        return i18n::tr("telemetry.stream_local", {{"buffer", integer(buffer_ms)}});
    return i18n::tr("telemetry.stream_network", {{"rate", decimal(read_mbps)},
                                                 {"buffer", integer(buffer_ms)},
                                                 {"bytes", decimal(buffered_bytes / 1048576.0)},
                                                 {"underruns", std::to_string(underruns)},
                                                 {"rebuffers", std::to_string(rebuffers)}});
}
std::vector<std::pair<std::string, std::string>> Telemetry::rows(double xr_fps, double target_fps, std::uint64_t skipped) const {
    const auto decimal = [](double value) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(1) << value;
        return out.str();
    };
    const auto integer = [](double value) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(0) << value;
        return out.str();
    };
    std::ostringstream signed_drift;
    signed_drift << std::showpos << std::fixed << std::setprecision(0) << sync.drift_ms;
    const auto media = i18n::tr("telemetry.media", {{"kind", i18n::tr(remote ? "telemetry.network" : "telemetry.local")},
                                                    {"bitrate", bitrate_mbps > 0 ? decimal(bitrate_mbps) + " Mbit/s" : i18n::tr("telemetry.bitrate_unknown")}});
    return {{i18n::tr("info.decode"), i18n::tr("telemetry.decode", {{"decoder", decoder_label(decoder)}, {"status", decoder_status(decoder)}})},
            {i18n::tr("info.render"), i18n::tr("telemetry.render", {{"fps", decimal(xr_fps)}, {"target", integer(target_fps)}})},
            {i18n::tr("info.sync"), i18n::tr("telemetry.drift", {{"drift", signed_drift.str()}, {"worst", integer(sync.worst_ms)}, {"verdict", sync.verdict()}})},
            {i18n::tr("info.queue"),
             i18n::tr("telemetry.queue",
                      {{"queue", std::to_string(sync.video_queue)}, {"dropped", std::to_string(sync.dropped)}, {"skipped", std::to_string(skipped)}, {"late", std::to_string(sync.late)}})},
            {i18n::tr("info.media"), media},
            {i18n::tr("info.stream"), stream.label(remote)}};
}
std::string frame_diagnostic(const RetainedFrame& frame, const DecoderDescription& decoder) {
    std::ostringstream out;
    const auto color = frame.color();
    out << "Decoder: " << decoder.name << " hardware=" << (decoder.hardware ? "yes" : "no") << " frame=" << (frame.software() ? "software" : "DMA-BUF") << " format=" << frame.format()
        << " size=" << frame.width() << 'x' << frame.height() << " depth=" << color.bits << " matrix=" << frame.colorspace() << " transfer=" << color.transfer << " primaries=" << color.primaries
        << " range=" << frame.color_range() << " chroma_location=" << color.chroma_location << " pts=" << std::fixed << std::setprecision(6) << frame.pts();
    return out.str();
}
}  // namespace lumaframevr::diagnostics
