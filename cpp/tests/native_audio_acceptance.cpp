#include "native_audio_acceptance.hpp"
#include "test_assertions.hpp"
#include "media_session.hpp"
#include "media_result.hpp"
#include "audio_internal.h"
#include <algorithm>
#include <array>
#include <limits>

void native_audio_acceptance() {
    {
        auto state = std::make_unique<Media>();
        state->references = 1;
        state->file = avformat_alloc_context();
        require(state->file != nullptr, "silent audio fixture allocates container");
        auto* stream = avformat_new_stream(state->file, nullptr);
        require(stream != nullptr, "silent audio fixture allocates video stream");
        stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        state->duration = 3.25;
        state->metadata[0] = 2;
        state->metadata[1] = 1;
        state->metadata[2] = 0;
        ::media::Session session(state.release());
        require(session.duration() == 3.25, "typed duration preserves container value");
        require(session.metadata() == std::array<int, 3>{2, 1, 0}, "typed metadata preserves fields");
        require(session.preview().status == 0 && session.get()->preview_requested, "typed preview requests frame");
        session.get()->preview_requested = 0;
        require(session.preview().status == 0 && session.get()->preview_requested, "repeated typed preview requests frame");
        ::media::Session closed(nullptr);
        const auto closed_preview = closed.preview();
        require(closed_preview.status == -1 && closed_preview.error == ::media::failure("preview on a closed decoder", AVERROR(EINVAL)).error, "closed preview preserves diagnostic");
        require(session.enable_audio().status == 0 && !session.has_audio(), "silent file needs no audio device");
        const auto tick = session.audio(false);
        require(tick.result.status == 0 && tick.clock == -1 && tick.limit == 0 && tick.done == 1, "silent typed audio tick preserves defaults");
        const auto no_volume = session.volume(0.5f, false);
        require(no_volume.status == -1 && no_volume.error == "no audio stream", "missing audio volume diagnostic unchanged");
        session.get()->audio_unavailable = 1;
        const auto unavailable = session.audio(false);
        require(unavailable.result.status == 0 && unavailable.clock == -1 && unavailable.limit == 0 && unavailable.done == 1, "typed unavailable audio finishes silently with default clocks");
        auto audio = std::make_unique<Audio>();
        audio->decoder = avcodec_alloc_context3(nullptr);
        audio->frame = av_frame_alloc();
        audio->pcm = av_frame_alloc();
        audio->packet = av_packet_alloc();
        require(audio->decoder && audio->frame && audio->pcm && audio->packet, "partial audio fixture allocates owned resources");
        session.get()->audio = audio.release();
        session.stop();
        require(!session.get()->audio && !session.get()->reader, "typed stop releases partial audio ownership");
        session.stop();
    }
    {
        AudioOutput output{};
        output.reset_to(1.25);
        require(output.reset && output.clock == 1.25 && output.end == 1.25 && !output.drained, "typed audio reset preserves target and flags");
        output.reset_to(2.5);
        require(output.reset && output.clock == 2.5 && output.end == 2.5 && !output.drained, "repeated typed audio reset preserves target and flags");
        output.reset = 0;
        output.corked = 1;
        require(output.control(true) == 1, "settled paused control needs no device operation");
        require(output.finish() == 1 && output.drained && output.clock == output.end, "empty output drains without a device");
        output.end = 3;
        output.drained = -1;
        require(output.finish() == AVERROR_EXTERNAL, "failed drain preserves error");
        output.drained = 0;
        require(output.finish() == 0, "corked output waits for drain");
        const auto time = output.time();
        require(time.status == 0 && time.clock == 2.5 && time.limit == 2.5, "typed inactive output clocks unchanged");
        auto partial = std::make_unique<AudioOutput>();
        partial->loop = pa_mainloop_new();
        require(partial->loop != nullptr, "partial output fixture allocates mainloop without connecting a device");
        partial->context = pa_context_new(pa_mainloop_get_api(partial->loop), "LumaFrameVR test");
        require(partial->context != nullptr, "partial output fixture allocates unconnected context");
        partial.reset();
    }
    {
        Audio input{};
        input.frame = av_frame_alloc();
        require(input.frame != nullptr, "PCM input fixture allocates frame");
        const AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        input.frame->format = AV_SAMPLE_FMT_FLT;
        input.frame->sample_rate = AUDIO_RATE;
        input.frame->ch_layout = stereo;
        input.frame->nb_samples = 4;
        require(av_frame_get_buffer(input.frame, 0) == 0, "PCM input fixture allocates samples");
        const std::array<float, 8> samples{0.25f, -0.25f, 0.5f, -0.5f, 0.75f, -0.75f, 1.0f, -1.0f};
        std::copy(samples.begin(), samples.end(), reinterpret_cast<float*>(input.frame->data[0]));
        Audio converted{};
        converted.pcm = av_frame_alloc();
        require(converted.pcm != nullptr, "PCM conversion fixture allocates frame");
        converted.end = std::numeric_limits<double>::infinity();
        require(converted.convert_to_playback_format(input.frame, 0) == 1 && converted.resampler && converted.input_rate == AUDIO_RATE && converted.pcm->nb_samples == 4 &&
                        converted.frame_start == 0 && !converted.offset,
                "real FFmpeg PCM conversion preserves sample count and timing");
        require(std::equal(samples.begin(), samples.end(), reinterpret_cast<float*>(converted.pcm->data[0])), "same-format PCM conversion preserves exact samples");
        require(converted.convert_to_playback_format(nullptr, 4.0 / AUDIO_RATE) == 1 && converted.source_ended && converted.pcm->nb_samples == 0, "resampler drain reports source end");
        Audio empty{};
        empty.pcm = av_frame_alloc();
        require(empty.pcm && empty.convert_to_playback_format(nullptr, 0) == 1 && empty.source_ended && !empty.resampler, "empty audio conversion finishes without allocating resampler");
    }
    {
        AudioOutput output{};
        const std::array<float, 8> samples{0.25f, -0.25f, 0.5f, -0.5f, 0.75f, -0.75f, 1.0f, -1.0f};
        require(output.set_volume(2.0f) == 0 && output.volume == 1 && output.set_volume(-1.0f) == 0 && output.volume == 0, "volume clamp unchanged");
        output.set_volume(1.0f);
        const auto unchanged = output.apply_gain(samples);
        require(unchanged.status == 0 && unchanged.samples.data() == samples.data(), "unity gain borrows source without allocation");
        const float* absent = nullptr;
        require(output.apply_gain_samples(absent, 4) == 0 && !absent, "unity typed gain does not dereference absent samples");
        output.set_volume(0.5f);
        const auto scaled = output.apply_gain(samples);
        require(scaled.status == 1 && scaled.samples.data() != samples.data() && scaled.samples.size() == samples.size(), "typed gain owns scaled storage");
        for(size_t i = 0; i < samples.size(); ++i) require(scaled.samples[i] == samples[i] * 0.5f, "gain preserves exact per-sample multiply");
        const auto reused = output.apply_gain(samples);
        require(reused.status == 1 && reused.samples.data() == scaled.samples.data() && samples[0] == 0.25f, "typed gain reuses storage without mutating source");
        const auto smaller = output.apply_gain(std::span<const float>(samples).first(2));
        require(smaller.status == 1 && smaller.samples.data() == reused.samples.data() && smaller.samples.size() == 2, "smaller gain batches retain storage");
        const auto incomplete = output.apply_gain(std::span<const float>(samples).first(3));
        require(incomplete.status == 1 && incomplete.samples.size() == 2, "typed gain returns only complete stereo frames");
        require(output.apply_gain_samples(absent, 0) == 0 && output.apply_gain_samples(absent, -1) == 0 && !absent, "empty and negative typed batches leave samples untouched");
        require(output.set_muted(-2) == 0 && output.muted == 1, "nonzero mute normalizes to one");
        const auto muted = output.apply_gain(samples);
        require(muted.status == 1, "muted gain scales samples");
        for(float sample : muted.samples) require(sample == 0.0f, "mute multiplies finite samples by zero");
        output.set_muted(false);
        output.set_volume(std::numeric_limits<float>::quiet_NaN());
        require(std::isnan(output.volume), "NaN volume is not clamped");
        const auto nan_gain = output.apply_gain(samples);
        require(nan_gain.status == 1 && std::isnan(nan_gain.samples[0]), "NaN gain preserves floating-point multiplication");
    }
}
