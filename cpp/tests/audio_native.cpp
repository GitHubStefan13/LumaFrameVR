#include "test_registry.hpp"
#include "test_assertions.hpp"
#include "audio_internal.h"
#include "audio_test_output.hpp"
#include "media_packets.hpp"
#include "media_session.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace {
constexpr int fixture_frames = 2 * AUDIO_RATE;
int16_t sample_at(int frame, int channel) {
    return static_cast<int16_t>((frame * 17 + channel * 3000) % 30000 - 15000);
}
void write_wav(const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    require(file.is_open(), "create local PCM reference fixture");
    auto little = [&](uint32_t value, int bytes) {
        for(int i = 0; i < bytes; ++i) file.put(static_cast<char>((value >> (i * 8)) & 255));
    };
    constexpr uint32_t bytes = fixture_frames * AUDIO_CHANNELS * sizeof(int16_t);
    // WAVE_FORMAT_EXTENSIBLE carries an explicit stereo mask, matching the
    // playback API's channel-layout contract (plain PCM WAV leaves it unknown).
    file.write("RIFF", 4); little(60 + bytes, 4); file.write("WAVEfmt ", 8);
    little(40, 4); little(0xfffe, 2); little(AUDIO_CHANNELS, 2); little(AUDIO_RATE, 4);
    little(AUDIO_RATE * AUDIO_CHANNELS * sizeof(int16_t), 4);
    little(AUDIO_CHANNELS * sizeof(int16_t), 2); little(16, 2);
    little(22, 2); little(16, 2); little(3, 4);
    little(1, 4); little(0, 2); little(0x10, 2);
    for(auto byte : {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}) little(byte, 1);
    file.write("data", 4); little(bytes, 4);
    for(int frame = 0; frame < fixture_frames; ++frame)
        for(int channel = 0; channel < AUDIO_CHANNELS; ++channel)
            little(static_cast<uint16_t>(sample_at(frame, channel)), 2);
    require(file.good(), "write complete PCM reference fixture");
}
::media::Session open_audio(const std::filesystem::path& path) {
    auto state = std::make_unique<Media>();
    state->references = 1;
    state->stream = state->audio_stream = -1;
    state->seek_target = -1;
    ::media::Session session(state.release());
    require(avformat_open_input(&session.get()->file, path.c_str(), nullptr, nullptr) == 0, "open real WAV container");
    require(avformat_find_stream_info(session.get()->file, nullptr) >= 0, "inspect real WAV stream");
    const auto enabled = session.enable_audio();
    require(enabled.status == 1 && enabled.error.empty() && session.has_audio(), "typed audio enables real decoder with test-only output");
    return session;
}
void seek_audio(::media::Session& session, double target) {
    auto* m = session.get();
    media_reader_pause(m, 1);
    require(audio_reset(m->audio, target) == 0, "seek resets decoder and synchronously confirms pause");
    media_clear_packets(m, AV_NOPTS_VALUE);
    const auto* stream = m->file->streams[m->audio_stream];
    const auto timestamp = av_rescale_q(static_cast<int64_t>(std::llround(target * AV_TIME_BASE)), AV_TIME_BASE_Q, stream->time_base);
    require(av_seek_frame(m->file, m->audio_stream, timestamp, AVSEEK_FLAG_BACKWARD) >= 0, "seek real PCM container");
    media_reader_pause(m, 0);
}
void compare_pcm(::media::Session& session, double target) {
    auto* m = session.get();
    auto& audio = *m->audio;
    int64_t last{};
    const auto first_reference = static_cast<int64_t>(std::llround(target * AUDIO_RATE));
    const auto deadline = av_gettime_relative() + 5000000;
    while(!audio.source_ended) { // Replaces the removed Audio::ended member.
        require(av_gettime_relative() < deadline, "PCM decode reaches EOF within deadline");
        const int decoded = audio.decode(*m);
        require(decoded >= 0, "decode real PCM through current Audio API: " + std::to_string(decoded) + " target=" + std::to_string(target));
        if(!audio.offset && audio.pcm->nb_samples) {
            const int first = audio.frame_start < 0 ? static_cast<int>(-audio.frame_start) : 0;
            const int64_t start = audio.frame_start + first;
            if(first < audio.pcm->nb_samples) {
                require(std::abs(start - last) <= 1, "PCM timestamps are contiguous");
                const auto* samples = reinterpret_cast<const float*>(audio.pcm->data[0]);
                for(int i = first; i < audio.pcm->nb_samples; ++i) {
                    const auto reference = first_reference + audio.frame_start + i;
                    require(reference >= 0 && reference < fixture_frames, "decoded PCM remains within reference length");
                    for(int channel = 0; channel < AUDIO_CHANNELS; ++channel)
                        near(samples[i * AUDIO_CHANNELS + channel], sample_at(static_cast<int>(reference), channel) / 32768.0,
                             1e-5, "decoded PCM matches independent integer reference");
                }
                last = audio.frame_start + audio.pcm->nb_samples;
            }
            audio.offset = audio.pcm->nb_samples;
        }
        av_usleep(100);
    }
    require(last > 0 && first_reference + last == fixture_frames, "PCM drain matches complete reference length");
}
::media::AudioTick tick(::media::Session& session, bool paused) {
    const auto result = session.audio(paused);
    require(result.result.status >= 0 && result.result.error.empty(), "typed audio tick succeeds");
    require(result.limit >= result.clock && result.limit <= session.get()->audio->output->end, "audio limit bounds playback clock");
    if(session.get()->audio->output->corked) require(result.limit == result.clock, "corked output cannot advance limit");
    return result;
}
double play_to(::media::Session& session, lumaframevr::tests::AudioTestOutput& output, double target) {
    const auto deadline = av_gettime_relative() + 5000000;
    ::media::AudioTick result;
    do {
        require(av_gettime_relative() < deadline, "audio playback progresses within deadline");
        output.advance(0.001);
        result = tick(session, false);
        av_usleep(100);
    } while(result.clock < target && !result.done);
    return result.clock;
}
}  // namespace

int lumaframevr::tests::audio_native(int argc, char** argv) {
    return acceptance_main([&] {
        require(argc == 2, "audio fixture directory argument");
        const auto directory = std::filesystem::path(argv[1]);
        std::filesystem::create_directories(directory);
        const auto path = directory / "reference.wav";
        write_wav(path);
        AudioTestOutput output;
        {
            auto session = open_audio(path);
            compare_pcm(session, 0);
            seek_audio(session, 0.25);
            output.assert_paused();
            compare_pcm(session, 0.25);
            seek_audio(session, 0);
            for(int i = 0; i < 400; ++i) tick(session, true);
            const auto initial = tick(session, true);
            require(initial.clock == 0 && !initial.done, "paused start holds clock at zero");
            require(play_to(session, output, 0.3) >= 0.3, "audio playback starts");
            output.advance(0.6);
            require(session.get()->audio->output->poll(false) >= 0, "starved output remains healthy");
            const auto starved = session.get()->audio->output->time();
            require(starved.status == 0 && starved.clock == starved.limit && starved.limit == session.get()->audio->output->end,
                    "starvation stops clock at queued end");
            for(int i = 0; i < 100; ++i) tick(session, true);
            const auto paused = tick(session, true).clock;
            for(int i = 0; i < 200; ++i) { output.advance(0.001); tick(session, true); }
            near(tick(session, true).clock, paused, 0.001, "pause keeps audio clock stable");
            for(int i = 0; i < 4; ++i) {
                const double target = i % 2 ? 0.1 : 1.0;
                seek_audio(session, target);
                output.assert_paused();
                for(int j = 0; j < 200; ++j) tick(session, true);
                const auto held = tick(session, true);
                require(held.clock == target && !held.done, "paused seek holds target");
                require(play_to(session, output, target + 0.1) >= target + 0.1, "seek resumes audio playback");
            }
            const auto end = play_to(session, output, 20);
            require(session.get()->audio->output->drained == 1 && end > 1, "EOF drains real decoded audio");
            seek_audio(session, 0.25);
            require(play_to(session, output, 0.35) >= 0.35, "seek after EOF restarts audio");
            seek_audio(session, 2);
            const auto deadline = av_gettime_relative() + 5000000;
            ::media::AudioTick eof;
            do {
                require(av_gettime_relative() < deadline, "paused seek to EOF completes");
                eof = tick(session, true);
                av_usleep(100);
            } while(!eof.done);
            require(eof.clock == 2, "paused seek to EOF never starts output");
            output.connected = false;
            require(session.get()->audio->output->poll(false) == AVERROR_EXTERNAL, "disconnected output reports low-level error");
            // Current typed policy continues video in silence instead of failing the session.
            const auto disconnected = session.audio(false);
            require(disconnected.result.status == 0 && disconnected.done && session.get()->audio_unavailable,
                    "typed session degrades to silent playback on output loss");
            session.stop();
            require(!session.get()->audio && !session.get()->reader, "stop releases audio and reader ownership");
        }
        output.connected = true;
        // No audio boundary call is needed for a video-only container.
        auto state = std::make_unique<Media>();
        state->references = 1;
        state->audio_stream = -1;
        state->file = avformat_alloc_context();
        require(state->file != nullptr, "allocate silent container");
        ::media::Session silent(state.release());
        auto* stream = avformat_new_stream(silent.get()->file, nullptr);
        require(stream != nullptr, "allocate silent video stream");
        stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        stream->codecpar->codec_id = AV_CODEC_ID_H264;
        require(silent.enable_audio().status == 0 && !silent.has_audio() && !silent.get()->audio, "silent media needs no audio server");
        const auto silence = silent.audio(false);
        require(silence.result.status == 0 && silence.done && silence.clock == -1 && silence.limit == 0, "typed silent tick reports finished audio");
    });
}
