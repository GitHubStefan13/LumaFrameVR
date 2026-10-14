#pragma once
#include "media_source.hpp"
#include "retained_frame.hpp"
#include <array>
#include <memory>
#include <optional>
#include <string>

namespace lumaframevr {
struct AudioObservation {
    std::optional<double> clock;
    double limit{};
    bool done{}, progress{};
    std::optional<double> position(double elapsed) const;
};
struct DecoderDescription {
    std::string name;
    bool hardware{};
    int width{}, height{}, bits{};
    double fps{}, bitrate_mbps{};
};
struct PacketBuffer {
    long long video_us{}, audio_us{};
    unsigned long long bytes{};
    bool ready{}, limited{}, waiting{};
};
enum class DecodeStatus { end, picture, pending, preview };
struct DecodeResult {
    DecodeStatus status;
    RetainedFrame frame;
};
class DecoderSession {
   public:
    explicit DecoderSession(const MediaSource& source, bool software = false);
    ~DecoderSession();
    DecoderSession(DecoderSession&&) noexcept;
    DecoderSession& operator=(DecoderSession&&) noexcept;
    DecoderSession(const DecoderSession&) = delete;
    DecoderSession& operator=(const DecoderSession&) = delete;
    DecodeResult advance();
    RetainedFrame next_frame();
    void seek(double seconds);
    void preview();
    bool reconfigure(bool released);
    std::optional<double> duration() const;
    std::array<int, 3> metadata() const;
    DecoderDescription description() const;
    PacketBuffer buffer(bool prefill);
    void enable_audio();
    AudioObservation audio(bool paused);
    void volume(float level, bool muted);

   private:
    struct Session;
    std::unique_ptr<Session> session_;
};
}  // namespace lumaframevr
