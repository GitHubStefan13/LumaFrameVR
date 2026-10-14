#include "decode_worker_state.hpp"
#include <utility>

namespace lumaframevr {
void DecodeWorker::decode(const MediaSource& source, bool software, const std::shared_ptr<Shared>& shared) {
    DecoderSession decoder(source, software);
    decoder.enable_audio();
    auto duration = decoder.duration();
    auto metadata = decoder.metadata();
    auto description = decoder.description();
    std::unique_lock lock(shared->mutex);
    shared->status.duration = duration;
    shared->status.metadata = metadata;
    shared->status.decoder = description;
    bool video_ended = false, reported_end = false;
    for(;;) {
        if(shared->stopped)
            return;
        if(shared->status.release_required) {
            shared->changed.wait(lock, [&] { return shared->stopped || !shared->status.release_required; });
            continue;
        }
        const auto generation = shared->status.generation;
        auto seek = std::exchange(shared->seek, {});
        auto volume = std::exchange(shared->volume, {});
        const bool preview = std::exchange(shared->preview, false);
        const bool prefill = !shared->running;
        const bool paused = shared->status.paused || prefill;
        const bool room = shared->frames.size() < 2;
        lock.unlock();
        if(seek) {
            decoder.seek(*seek);
            video_ended = false;
            reported_end = false;
        }
        decoder.reconfigure(true);
        if(volume)
            decoder.volume(volume->first, volume->second);
        if(preview)
            decoder.preview();
        auto audio = decoder.audio(paused);
        auto observed = std::chrono::steady_clock::now();
        auto result = !video_ended && room ? decoder.advance() : DecodeResult{DecodeStatus::pending, {}};
        bool changed = decoder.reconfigure(false);
        auto buffer = decoder.buffer(prefill);
        description = decoder.description();
        lock.lock();
        if(shared->stopped || shared->status.generation != generation) {
            lock.unlock();
            result.frame = {};
            lock.lock();
            continue;
        }
        shared->status.decoder = std::move(description);
        shared->status.buffer = buffer;
        if(changed) {
            std::deque<DecodeResult> retired;
            retired.swap(shared->frames);
            shared->status.queue_depth = 0;
            shared->status.release_required = true;
            lock.unlock();
            result.frame = {};
            retired.clear();
            lock.lock();
            continue;
        }
        shared->status.ready = buffer.ready;
        shared->status.audio = audio;
        shared->audio_observed = audio.clock ? std::optional(observed) : std::nullopt;
        if(result.status == DecodeStatus::end) {
            video_ended = true;
            shared->running = true;
        } else if(result.status != DecodeStatus::pending) {
            shared->frames.push_back(std::move(result));
        }
        if(video_ended && audio.done && !reported_end && shared->frames.size() < 2) {
            shared->frames.push_back({DecodeStatus::end, {}});
            reported_end = true;
        }
        shared->status.queue_depth = shared->frames.size();
        if(!audio.progress && (buffer.waiting || video_ended || shared->frames.size() >= 2))
            shared->changed.wait_for(lock, std::chrono::milliseconds(5));
    }
}
}  // namespace lumaframevr
