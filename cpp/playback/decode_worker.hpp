#pragma once
#include "decoder_session.hpp"
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace lumaframevr {
struct WorkerSnapshot {
    std::size_t queue_depth{};
    std::uint64_t generation{};
    bool release_required{}, ready{}, paused{};
    std::optional<double> duration;
    std::array<int, 3> metadata{-1, -1, -1};
    DecoderDescription decoder;
    PacketBuffer buffer;
    AudioObservation audio;
};
class DecodeWorker {
   public:
    explicit DecodeWorker(MediaSource source, bool software = false, std::optional<std::pair<float, bool>> initial_volume = {});
    ~DecodeWorker();
    DecodeWorker(const DecodeWorker&) = delete;
    DecodeWorker& operator=(const DecodeWorker&) = delete;
    WorkerSnapshot snapshot() const;
    DecodeResult next();
    std::uint64_t seek(double target);
    void released(std::uint64_t generation);
    void pause(bool paused);
    void begin();
    void preview();
    void volume(float level, bool muted);
    std::optional<double> audio_clock() const;
    void stop();

   private:
    struct Shared;
    static void decode(const MediaSource& source, bool software, const std::shared_ptr<Shared>& shared);
    std::shared_ptr<Shared> shared_;
    std::thread thread_;
};
}  // namespace lumaframevr
