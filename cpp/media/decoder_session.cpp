#include "decoder_internal.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace lumaframevr {
DecoderSession::DecoderSession(const MediaSource& source, bool software) {
    const auto resolved = source.resolved();
    auto result = media::open(resolved.input().c_str(), software);
    if(auto* error = std::get_if<media::Error>(&result))
        throw std::runtime_error(error->message);
    session_ = std::make_unique<Session>(std::move(std::get<media::Session>(result)));
}
DecoderSession::~DecoderSession() = default;
DecoderSession::DecoderSession(DecoderSession&&) noexcept = default;
DecoderSession& DecoderSession::operator=(DecoderSession&&) noexcept = default;
DecodeResult DecoderSession::advance() {
    media::Frame frame;
    const auto result = session_->native.next(frame);
    check_native(result.status, result.error.c_str());
    switch(result.status) {
        case 0:
            return {DecodeStatus::end, {}};
        case 2:
            return {DecodeStatus::pending, {}};
        case 1:
        case 3:
            return {result.status == 1 ? DecodeStatus::picture : DecodeStatus::preview, RetainedFrame(std::make_unique<RetainedFrame::Storage>(std::move(frame)))};
        default:
            throw std::runtime_error("Native decoder returned an unknown status");
    }
}
RetainedFrame DecoderSession::next_frame() {
    for(;;) {
        reconfigure(true);
        auto decoded = advance();
        if(decoded.status == DecodeStatus::picture)
            return std::move(decoded.frame);
        if(decoded.status == DecodeStatus::end)
            return {};
        if(decoded.status == DecodeStatus::pending && buffer(false).waiting)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
std::optional<double> DecoderSession::duration() const {
    const auto seconds = session_->native.duration();
    return std::isfinite(seconds) && seconds > 0 ? std::optional(seconds) : std::nullopt;
}
std::array<int, 3> DecoderSession::metadata() const {
    return session_->native.metadata();
}
DecoderDescription DecoderSession::description() const {
    const auto info = session_->native.info();
    const auto end = std::find(std::begin(info.name), std::end(info.name), '\0');
    return {std::string(info.name, end), info.hardware != 0, info.width, info.height, info.bits, info.fps, info.bitrate_mbps};
}
PacketBuffer DecoderSession::buffer(bool prefill) {
    auto value = session_->native.buffer(prefill);
    return {value.video_us, value.audio_us, value.bytes, value.ready != 0, value.limited != 0, value.waiting != 0};
}
}  // namespace lumaframevr
