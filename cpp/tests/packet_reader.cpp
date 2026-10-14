#include "test_registry.hpp"
#include "test_assertions.hpp"
#include "media_packets.hpp"
#include "media_owned_frame.hpp"
#include "media_session.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <thread>

namespace {
AVFormatContext* intercepted{};
std::atomic<bool> gated{}, reading{};
std::atomic<int> calls{}, failure{};
int video_cursor{}, audio_cursor{};

void wait_for(const auto& predicate, const char* message) {
    const auto deadline = av_gettime_relative() + 5000000;
    while(!predicate()) {
        require(av_gettime_relative() < deadline, message);
        av_usleep(1000);
    }
}
struct ReleaseGate {
    ~ReleaseGate() { gated = false; }
};
class Worker {
   public:
    template <class Function>
    explicit Worker(Function function)
        : thread_([this, function] {
              function();
              done = true;
          }) {}
    ~Worker() {
        gated = false;
        thread_.join();
    }
    std::atomic<bool> done{};

   private:
    std::thread thread_;
};
uint64_t digest(const AVPacket& packet) {
    uint64_t value = static_cast<uint64_t>(packet.pts);
    for(int i = 0; i < packet.size; ++i) value = value * 31 + packet.data[i];
    return value;
}
::media::Session synthetic_session() {
    auto state = std::make_unique<Media>();
    state->references = 1;
    state->file = avformat_alloc_context();
    require(state->file != nullptr, "allocate reader container");
    ::media::Session session(state.release());
    auto* m = session.get();
    m->stream = 0;
    m->audio_stream = 1;
    m->seek_target = -1;
    m->trace_enabled = 1;  // Replaces the removed Media::tracing field.
    for(int i = 0; i < 2; ++i) {
        auto* stream = avformat_new_stream(m->file, nullptr);
        require(stream != nullptr, "allocate reader streams");
        stream->time_base = {1, 1000};
        stream->start_time = 0;
        stream->duration = i ? 500 : 4000;
    }
    m->packet = av_packet_alloc();
    require(m->packet != nullptr, "allocate consumer packet");
    return session;
}
void rewind_synthetic(::media::Session& session) {
    media_reader_pause(session.get(), 1);
    media_clear_packets(session.get(), AV_NOPTS_VALUE);
    video_cursor = audio_cursor = 0;
    media_reader_pause(session.get(), 0);
}
}  // namespace

extern "C" int __real_av_read_frame(AVFormatContext*, AVPacket*);
extern "C" int __wrap_av_read_frame(AVFormatContext* file, AVPacket* packet) {
    if(file != intercepted)
        return __real_av_read_frame(file, packet);
    reading = true;
    ++calls;
    while(gated) av_usleep(1000);
    int result = failure.exchange(0);
    if(!result) {
        if(video_cursor == 40 && audio_cursor == 5) {
            result = AVERROR_EOF;
        } else {
            const bool audio = audio_cursor < 5 && audio_cursor <= video_cursor;
            const int index = audio ? audio_cursor++ : video_cursor++;
            result = av_new_packet(packet, 8);
            if(result >= 0) {
                packet->stream_index = audio ? 1 : 0;
                packet->pts = packet->dts = index * 100;
                packet->duration = 100;
                for(int i = 0; i < packet->size; ++i) packet->data[i] = static_cast<uint8_t>(index * 7 + i + (audio ? 99 : 0));
            }
        }
    }
    reading = false;
    return result;
}

int lumaframevr::tests::packet_reader(int, char**) {
    return acceptance_main([] {
        auto session = synthetic_session();
        intercepted = session.get()->file;
        ReleaseGate release;
        std::array<uint64_t, 2> expected{}, actual{};
        while(__wrap_av_read_frame(intercepted, session.get()->packet) >= 0) {
            auto& packet = *session.get()->packet;
            const auto stream = packet.stream_index;
            expected[stream] = expected[stream] * 31 + digest(packet);
            av_packet_unref(&packet);
        }
        video_cursor = audio_cursor = 0;
        gated = true;
        require(media_reader_start(session.get()) == 0, "start real reader worker");
        wait_for([] { return reading.load(); }, "worker reaches blocked read");
        auto started = av_gettime_relative();
        require(::media::packet(*session.get(), 0, *session.get()->packet) == AVERROR(EAGAIN), "blocked read does not block dequeue");
        require(av_gettime_relative() - started < 100000, "empty dequeue returns promptly");
        {
            Worker pause([&] { media_reader_pause(session.get(), 1); });
            av_usleep(20000);
            require(!pause.done, "pause waits for in-flight read");
            gated = false;
            wait_for([&] { return pause.done.load(); }, "pause completes after read is released");
        }
        rewind_synthetic(session);
        wait_for([&] { return session.buffer(false).ready; }, "reader reaches one-second prefill");
        const auto buffer = session.buffer(false);
        require(buffer.video_us >= READ_AHEAD_US && buffer.audio_us == 500000, "short ended audio does not prevent video prefill");
        int count = calls;
        av_usleep(30000);
        require(calls == count, "filled reader is idle");
        auto take_video = [&] {
            require(::media::packet(*session.get(), 0, *session.get()->packet) == 0, "queued video is available");
            actual[0] = actual[0] * 31 + digest(*session.get()->packet);
        };
        for(int i = 0; i < 5; ++i) take_video();
        require(!session.buffer(true).ready, "startup refill bypasses playback hysteresis");
        wait_for([&] { return session.buffer(true).ready; }, "startup refill completes");
        gated = true;
        for(int i = 0; i < 6; ++i) take_video();
        wait_for([] { return reading.load(); }, "low-water refill reaches blocked read");
        require(session.buffer(false).video_us >= 250000, "queued packets remain during refill");
        started = av_gettime_relative();
        for(int i = 0; i < 3; ++i) take_video();
        require(av_gettime_relative() - started < 100000, "queued dequeue remains prompt during blocked refill");
        gated = false;
        std::array<bool, 2> ended{};
        wait_for(
                [&] {
                    for(int i = 0; i < 2; ++i) {
                        if(ended[i])
                            continue;
                        const int result = ::media::packet(*session.get(), i, *session.get()->packet);
                        require(result == 0 || result == AVERROR(EAGAIN) || result == AVERROR_EOF, "packet drain returns data/wait/EOF");
                        if(result == 0)
                            actual[i] = actual[i] * 31 + digest(*session.get()->packet);
                        ended[i] = result == AVERROR_EOF;
                    }
                    return ended[0] && ended[1];
                },
                "both streams drain to EOF");
        require(actual == expected, "packet bytes and order match reference on both streams");
        require(session.trace().packets > 0, "typed trace includes reader packets");
        media_reader_pause(session.get(), 1);
        media_clear_packets(session.get(), AV_NOPTS_VALUE);
        video_cursor = audio_cursor = 0;
        failure = AVERROR(EAGAIN);
        media_reader_pause(session.get(), 0);
        wait_for([&] { return session.buffer(false).ready; }, "transient EAGAIN retries successfully");
        media_reader_pause(session.get(), 1);
        media_clear_packets(session.get(), AV_NOPTS_VALUE);
        failure = AVERROR(EIO);
        media_reader_pause(session.get(), 0);
        int result = AVERROR(EAGAIN);
        wait_for(
                [&] {
                    result = ::media::packet(*session.get(), 0, *session.get()->packet);
                    return result != AVERROR(EAGAIN);
                },
                "permanent read error reaches consumer");
        require(result == AVERROR(EIO), "read error code is preserved");
        gated = true;
        rewind_synthetic(session);
        wait_for([] { return reading.load(); }, "stop fixture blocks reader");
        {
            Worker stop([&] { session.stop(); });
            av_usleep(20000);
            require(!stop.done, "stop waits for in-flight read");
            gated = false;
            wait_for([&] { return stop.done.load(); }, "stop joins released reader");
        }
        require(!session.get()->reader, "stop releases reader ownership");
        intercepted = nullptr;
        // Real local demux/decode and repeated seeks complement the gated I/O fixture.
        const char* path = std::getenv("LUMAFRAMEVR_TEST_VIDEO");
        if(!path)
            skip("LUMAFRAMEVR_TEST_VIDEO is not set");
        auto opened = ::media::open(path, true);
        require(std::holds_alternative<::media::Session>(opened), "typed real session opens without VPU");
        auto real = std::move(std::get<::media::Session>(opened));
        auto* state = real.get();
        state->audio_stream = av_find_best_stream(state->file, AVMEDIA_TYPE_AUDIO, -1, state->stream, nullptr, 0);
        std::array<uint64_t, 2> file_expected{}, file_actual{};
        std::array<size_t, 2> expected_count{}, actual_count{};
        int read_result;
        while((read_result = __real_av_read_frame(state->file, state->packet)) >= 0) {
            const int stream = state->packet->stream_index == state->stream ? 0 : state->packet->stream_index == state->audio_stream ? 1 : -1;
            if(stream >= 0) {
                file_expected[stream] = file_expected[stream] * 31 + digest(*state->packet);
                ++expected_count[stream];
            }
            av_packet_unref(state->packet);
        }
        require(read_result == AVERROR_EOF && expected_count[0] > 0, "real reference demux reaches EOF with video packets");
        require(av_seek_frame(state->file, state->stream, 0, AVSEEK_FLAG_BACKWARD) >= 0, "rewind real packet reference");
        std::array<bool, 2> file_ended{false, state->audio_stream < 0};
        wait_for(
                [&] {
                    for(int stream = 0; stream < 2; ++stream) {
                        if(file_ended[stream])
                            continue;
                        const int packet_result = ::media::packet(*state, stream, *state->packet);
                        require(packet_result == 0 || packet_result == AVERROR(EAGAIN) || packet_result == AVERROR_EOF, "real reader returns packet/wait/EOF");
                        if(packet_result == 0) {
                            file_actual[stream] = file_actual[stream] * 31 + digest(*state->packet);
                            ++actual_count[stream];
                        }
                        file_ended[stream] = packet_result == AVERROR_EOF;
                    }
                    return file_ended[0] && file_ended[1];
                },
                "real packet reader drains both streams");
        require(file_actual == file_expected && actual_count == expected_count, "real buffered packet bytes, timestamps, counts and order match direct FFmpeg demux");
        for(int i = 0; i < 20; ++i) {
            const double target = i % 2 ? 0.5 : 0;
            const auto seek = real.seek(target);
            require(seek.status == 0 && seek.error.empty(), "repeated typed seek/preroll succeeds: " + seek.error);
            ::media::Frame frame;
            wait_for(
                    [&] {
                        frame = {};  // Preview (status 3) also owns a frame; release before the next call.
                        const auto next = real.next(frame);
                        require(next.status >= 0 && next.error.empty(), "typed decode returns no error");
                        return next.status == 1;
                    },
                    "seeked frame arrives");
            require(frame.get().pts >= target, "seek discards preceding frames");
        }
    });
}
