#include "test_registry.hpp"
#include "decoder_session.hpp"
#include "test_assertions.hpp"
#include "media_session.hpp"
#include "media_owned_frame.hpp"
#include "media_errors.hpp"
#include "media_metadata.hpp"
#include "media_packets.hpp"
#include "media_codec.hpp"
#include "media_decoder_startup.hpp"
#include "native_audio_acceptance.hpp"
#include "application_options.hpp"
#include <cstdio>
#include <unistd.h>
#include <type_traits>
#include <cstdlib>
#include <vector>
#include <optional>

namespace {
class ScopedDecoderMode {
   public:
    explicit ScopedDecoderMode(const char* mode, const char* variable = "LUMAFRAMEVR_DECODER") : variable_(variable) {
        if(const char* previous = std::getenv(variable_))
            previous_ = previous;
        require(setenv(variable_, mode, 1) == 0, "decoder mode fixture sets environment");
    }
    ~ScopedDecoderMode() {
        if(previous_)
            setenv(variable_, previous_->c_str(), 1);
        else
            unsetenv(variable_);
    }
    ScopedDecoderMode(const ScopedDecoderMode&) = delete;
    ScopedDecoderMode& operator=(const ScopedDecoderMode&) = delete;

   private:
    const char* variable_;
    std::optional<std::string> previous_;
};
}  // namespace

int lumaframevr::tests::media(int, char**) {
    return acceptance_main([] {
        using namespace lumaframevr;
        const char* path = std::getenv("LUMAFRAMEVR_TEST_VIDEO");
        if(path == nullptr)
            skip("LUMAFRAMEVR_TEST_VIDEO is not set");
        auto source = MediaSource::parse(path).resolved();
        rejects([] { DecoderSession(MediaSource::parse("/nonexistent/video.mp4"), true); }, "open failure");
        RetainedFrame held;
        std::vector<unsigned char> original;
        {
            DecoderSession decoder(source, true);
            require(decoder.duration().value_or(0) >= 1.0, "real duration");
            held = decoder.next_frame();
            require(held.valid(), "decoded frame");
            require(held.width() == 96 && held.height() == 64, "real geometry");
            require(held.software(), "explicit software never probes VPU");
            original.assign(held.plane_row(0, 0).begin(), held.plane_row(0, 0).end());
            rejects([&] { decoder.seek(0.5); }, "seek requires releasing retained frames");
            held = {};
            decoder.seek(0.5);
            auto seeked = decoder.next_frame();
            require(seeked.pts() >= 0.5, "seek rejects preceding keyframes");
            seeked = {};
            decoder.seek(0);
            auto first = decoder.next_frame();
            require(std::equal(original.begin(), original.end(), first.plane_row(0, 0).begin()), "seek pixels equal");
            first = {};
            int count = 0;
            while(decoder.next_frame().valid()) ++count;
            require(count > 0, "drains through EOF");
            decoder.seek(0);
            held = decoder.next_frame();
            require(held.valid(), "seek restarts EOF");
        }
        require(std::equal(original.begin(), original.end(), held.plane_row(0, 0).begin()), "frame survives decoder destruction");
        RetainedFrame moved(std::move(held));
        require(!held.valid() && moved.valid(), "move transfers ownership once");
        int result;
        {
            auto opened = ::media::open(source.input().c_str(), true);
            require(std::holds_alternative<::media::Session>(opened), "typed native reference opens");
            auto session = std::move(std::get<::media::Session>(opened));
            ::media::Frame reference;
            do {
                result = session.next(reference).status;
            } while(result == 2);
            require(result == 1, "typed native reference frame");
            require(std::equal(original.begin(), original.end(), reference.get().data[0]), "C++ matches original native bytes");
            near(moved.pts(), reference.get().pts, 1e-9, "C++ matches reference PTS");
            reference = {};
            session.stop();
        }
        rejects(
                [&] {
                    DecoderSession decoder(source, true);
                    decoder.seek(-1);
                },
                "invalid seek");
        static_assert(!std::is_copy_constructible_v<::media::Session> && std::is_nothrow_move_constructible_v<::media::Session>);
        static_assert(!std::is_copy_constructible_v<::media::Frame> && std::is_nothrow_move_constructible_v<::media::Frame>);
        auto failed = ::media::open("/nonexistent/video.mp4", true);
        require(std::holds_alternative<::media::Error>(failed), "typed open returns error");
        require(std::get<::media::Error>(failed).code == AVERROR(ENOENT) && std::get<::media::Error>(failed).message.find("file=/nonexistent/video.mp4") != std::string::npos,
                "typed open preserves missing-file code and diagnostic");
        ::media::Frame retained;
        {
            auto opened = ::media::open(source.input().c_str(), true);
            require(std::holds_alternative<::media::Session>(opened), "typed session opens");
            auto session = std::move(std::get<::media::Session>(opened));
            require(std::get<::media::Session>(opened).get() == nullptr, "session move empties source");
            const auto description = session.info();
            require(description.width == 96 && description.height == 64 && description.bits == 8 && !description.hardware, "typed info returns software geometry and depth");
            require(std::string(description.name) == session.get()->info.name && description.fps == session.get()->info.fps && description.bitrate_mbps == session.get()->info.bitrate_mbps,
                    "typed description preserves decoder name, rate and bitrate");
            require(session.reconfigure(false).status == 0 && session.reconfigure(true).status == 0, "software reconfiguration remains a no-op");
            require(session.seek(0).status == 0, "typed seek succeeds");
            do {
                retained = {};
                result = session.next(retained).status;
            } while(result == 2 || result == 3);
            require(result == 1, "typed frame owns a decoded picture");
            const auto busy = session.seek(0.5);
            require(busy.status < 0 && busy.error.find("release decoded frames") != std::string::npos, "typed seek returns retained-frame error");
            ::media::Frame transferred(std::move(retained));
            require(!retained.get().owner && transferred.get().owner, "native frame move empties source");
            retained = std::move(transferred);
            auto replacement = ::media::open(source.input().c_str(), true);
            require(std::holds_alternative<::media::Session>(replacement), "replacement session opens");
            auto replacement_session = std::move(std::get<::media::Session>(replacement));
            replacement_session = std::move(session);
            require(!session.get() && replacement_session.get(), "session move assignment transfers ownership");
        }
        require(std::equal(original.begin(), original.end(), retained.get().data[0]), "native frame survives RAII session destruction");
        retained = {};
        char diagnostic[2048]{};
        const auto unsupported = ::media::unsupported_video(nullptr, "fixture.mp4");
        require(unsupported.status == -1 && unsupported_video(nullptr, "fixture.mp4", diagnostic, sizeof(diagnostic)) == -1, "unsupported error status unchanged");
        require(unsupported.error == diagnostic && unsupported.error.find("codec=none, profile=unknown, pixel_format=unknown, 0x0, depth=0") != std::string::npos,
                "unsupported diagnostic fields and facade agree");
        const auto startup = ::media::startup_failure(nullptr, "fixture.mp4", nullptr, AVERROR(EINVAL));
        require(startup_failure(nullptr, "fixture.mp4", nullptr, AVERROR(EINVAL), diagnostic, sizeof(diagnostic)) == -1 && startup.error == diagnostic, "startup default operation and facade agree");
        char truncated[17]{};
        startup_failure(nullptr, "fixture.mp4", nullptr, AVERROR(EINVAL), truncated, sizeof(truncated));
        require(std::string(truncated) == startup.error.substr(0, sizeof(truncated) - 1), "facade error truncation unchanged");
        AVCodecParameters parameters{};
        parameters.codec_id = AV_CODEC_ID_H264;
        parameters.format = AV_PIX_FMT_YUV420P;
        parameters.profile = -123;
        require(::media::unsupported_video(&parameters, "fixture.mp4").error.find("profile=-123") != std::string::npos, "unknown profile keeps numeric fallback");
        {
            Media metadata_fixture{};
            metadata_fixture.file = avformat_alloc_context();
            require(metadata_fixture.file != nullptr, "metadata fixture allocates container");
            auto* stream = avformat_new_stream(metadata_fixture.file, nullptr);
            require(stream != nullptr, "metadata fixture allocates stream");
            require(::media::read_metadata(metadata_fixture) == ::media::Metadata{-1, -1, -1}, "absent metadata stays unknown");
            av_dict_set(&metadata_fixture.file->metadata, "projection", "EQUIRECTANGULAR", 0);
            av_dict_set(&metadata_fixture.file->metadata, "stereo_mode", "top-bottom", 0);
            av_dict_set(&stream->metadata, "stereo_mode", "SIDE BY SIDE", 0);
            require(::media::read_metadata(metadata_fixture) == ::media::Metadata{2, 1, 0}, "metadata tag case and stream precedence unchanged");
            media_read_metadata(&metadata_fixture);
            require(std::equal(std::begin(metadata_fixture.metadata), std::end(metadata_fixture.metadata), ::media::Metadata{2, 1, 0}.begin()), "value metadata and stored fields agree");
            const std::string beyond_limit = std::string(63, 'x') + "mono";
            av_dict_set(&stream->metadata, "stereo_mode", beyond_limit.c_str(), 0);
            require(::media::read_metadata(metadata_fixture) == ::media::Metadata{2, 2, 0}, "metadata tag truncation remains 63 bytes");
        }
        {
            Media packet_fixture{};
            auto reader = std::make_unique<Reader>();
            packet_fixture.reader = reader.release();
            auto* r = packet_fixture.reader;
            require(pthread_mutex_init(&r->mutex, nullptr) == 0, "packet fixture initializes mutex");
            r->mutex_ready = 1;
            require(pthread_cond_init(&r->changed, nullptr) == 0, "packet fixture initializes condition");
            r->condition_ready = 1;
            r->stream[0] = 0;
            r->stream[1] = -1;
            r->time_base[0] = AVRational{1, 1000};
            r->end[0] = r->end[1] = AV_NOPTS_VALUE;
            r->packets[0] = av_fifo_alloc2(1, sizeof(AVPacket), 0);
            r->packets[1] = av_fifo_alloc2(1, sizeof(AVPacket), 0);
            r->pending = av_packet_alloc();
            packet_fixture.packet = av_packet_alloc();
            require(r->packets[0] && r->packets[1] && r->pending && packet_fixture.packet, "packet fixture allocates queues and packets");
            require(av_new_packet(r->pending, 8) == 0, "packet fixture allocates payload");
            r->pending->stream_index = 0;
            r->pending->pts = r->pending->dts = 1000;
            r->pending->duration = 100;
            r->pending->data[0] = 42;
            require(r->queue() == 0 && !r->pending->buf && r->bytes == 8, "queue transfers packet reference once");
            require(r->buffered(0) == 100000 && reader_buffered(r, 0) == r->buffered(0), "buffer duration and facade agree");
            require(av_new_packet(r->pending, 8) == 0, "packet fixture allocates second payload");
            r->pending->stream_index = 0;
            require(reader_queue(r) == AVERROR(EAGAIN) && r->limited && r->pending->buf, "full queue retains pending reference");
            require(reader_take(r, 0, packet_fixture.packet) == 0 && packet_fixture.packet->data[0] == 42 && r->bytes == 0, "take transfers queued payload");
            require(reader_queue(r) == 0 && r->bytes == 8, "queue resumes after take");
            media_clear_packets(&packet_fixture, AV_NOPTS_VALUE);
            require(r->bytes == 0 && !av_fifo_can_read(r->packets[0]) && !r->pending->buf && !r->limited && r->filling, "clear releases queued references and resets state");
            require(r->take(0, *packet_fixture.packet) == AVERROR(EAGAIN) && (r->waiting & 1), "empty queue preserves waiting result");
        }
        const auto support = ::media::supported(source.input().c_str());
        require(support.status == 1 && support.error.empty(), "typed inspection accepts real fixture");
        const auto missing = ::media::supported("/nonexistent/video.mp4");
        require(missing.status == -1 && missing.error == ::media::failure("inspect video", AVERROR(ENOENT)).error, "typed inspection preserves missing-file diagnostic");
        require(::media::decodable(parameters) && decodable(&parameters), "H264 8-bit 420 policy unchanged");
        parameters.format = AV_PIX_FMT_YUV420P10LE;
        require(::media::decodable(parameters) && !decoder_name(&parameters), "10-bit H264 remains software only");
        parameters.codec_id = AV_CODEC_ID_HEVC;
        require(::media::decodable(parameters) && std::string(decoder_name(&parameters)) == "hevc_v4l2m2m", "10-bit HEVC remains hardware eligible");
        parameters.format = AV_PIX_FMT_YUV444P;
        require(!::media::decodable(parameters) && !decoder_name(&parameters), "444 remains unsupported");
        parameters.format = AV_PIX_FMT_YUV420P;
        parameters.codec_id = AV_CODEC_ID_AV1;
        require(!::media::decodable(parameters), "AV1 remains outside supported policy");
        {
            ScopedDecoderMode mode("software");
            auto opened = ::media::open(source.input().c_str());
            require(std::holds_alternative<::media::Session>(opened), "forced software policy opens without VPU");
            auto session = std::move(std::get<::media::Session>(opened));
            auto* state = session.get();
            require(state->software_decoding && state->skip_hardware_decoder && !state->first_frame_observed, "forced software startup flags unchanged");
            const auto generation = state->decoder_generation;
            state->flushing_at_end = 1;
            require(::media::open_software_decoder(*state) == 0 && state->decoder_generation == generation + 1 && !state->flushing_at_end, "software replacement preserves generation and flush reset");
            require(state->decoder && state->decoder->apply_cropping == 0 && state->decoder->thread_type == (FF_THREAD_FRAME | FF_THREAD_SLICE), "software decoder options unchanged");
            require(open_software_decoder(state) == 0 && state->decoder_generation == generation + 2, "software C facade replaces decoder with same generation rule");
        }
        {
            ScopedDecoderMode mode("hardware");
            auto opened = ::media::open(source.input().c_str(), true);
            require(std::holds_alternative<::media::Session>(opened) && std::get<::media::Session>(opened).get()->software_decoding,
                    "explicit software session still overrides forced hardware without probing VPU");
        }
        for(const char* invalid_mode : {"invalid", ""}) {
            ScopedDecoderMode mode(invalid_mode);
            const auto failed_mode = ::media::open(source.input().c_str(), true);
            require(std::holds_alternative<::media::Error>(failed_mode) && std::get<::media::Error>(failed_mode).code == AVERROR(EINVAL), "invalid decoder mode keeps EINVAL");
            require(std::get<::media::Error>(failed_mode).message.find("open native decoder") != std::string::npos, "typed startup policy preserves operation diagnostic");
        }
        {
            Media failure_fixture{};
            failure_fixture.file = avformat_alloc_context();
            require(failure_fixture.file != nullptr, "startup failure fixture allocates container");
            auto* stream = avformat_new_stream(failure_fixture.file, nullptr);
            require(stream != nullptr, "startup failure fixture allocates stream");
            stream->codecpar->codec_id = AV_CODEC_ID_NONE;
            require(::media::open_software_decoder(failure_fixture) == AVERROR_DECODER_NOT_FOUND && !failure_fixture.decoder && !failure_fixture.decoder_generation,
                    "missing software decoder leaves session state untouched");
            stream->codecpar->codec_id = AV_CODEC_ID_H264;
            stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
            stream->codecpar->format = AV_PIX_FMT_YUV420P;
            failure_fixture.flushing_at_end = 1;
            require(::media::open_software_decoder(failure_fixture) < 0 && failure_fixture.decoder && failure_fixture.decoder_generation == 1 && failure_fixture.software_decoding &&
                            failure_fixture.flushing_at_end,
                    "failed software startup retains partial context for session teardown");
        }
        {
            // DecoderMode::Auto is the default path a plain launch takes, and it now prefers our
            // own V4L2 queue. Run before any hardware attempt can write the VPU off, so the
            // diagnostic proves the route rather than the fallback that follows a write-off: an
            // absent decoder device must produce the same V4L2 message an explicit --decoder v4l2
            // request produces.
            ScopedDecoderMode mode("auto");
            ScopedDecoderMode absent("/nonexistent/lumaframevr-v4l2-decoder", "LUMAFRAMEVR_V4L2_DEVICE");
            std::FILE* log = std::fopen("v4l2-auto-fallback.log", "w+");
            require(log != nullptr, "open auto fallback diagnostic capture");
            const int saved = dup(STDERR_FILENO);
            require(saved >= 0, "save stderr for the auto fallback diagnostic");
            std::fflush(stderr);
            require(dup2(fileno(log), STDERR_FILENO) >= 0, "capture the auto fallback diagnostic");
            auto opened = ::media::open(source.input().c_str());
            std::fflush(stderr);
            require(dup2(saved, STDERR_FILENO) >= 0, "restore stderr after the auto fallback startup");
            close(saved);
            std::rewind(log);
            std::string diagnostic;
            char bytes[1024];
            while(const auto count = std::fread(bytes, 1, sizeof(bytes), log)) diagnostic.append(bytes, count);
            std::fclose(log);
            require(std::holds_alternative<::media::Session>(opened), "auto prefers the V4L2 queue and still opens");
            require(diagnostic.find("own V4L2 unavailable") != std::string::npos && diagnostic.find("/nonexistent/lumaframevr-v4l2-decoder") != std::string::npos &&
                            diagnostic.find("falling back to FFmpeg software") != std::string::npos,
                    "auto routes through our own V4L2 queue before FFmpeg");
            auto session = std::move(std::get<::media::Session>(opened));
            require(!session.info().hardware, "auto's V4L2 fallback is the software path");
        }
        {
            const std::string_view arguments[]{"--decoder", "v4l2", path};
            const auto options = parse_application_options(arguments);
            require(options.decoder == "v4l2", "decoder option parses v4l2");
            for(const char* mode : {"auto", "hardware", "software", "v4l2"}) {
                ScopedDecoderMode environment("invalid");
                const std::string_view selected[]{"--decoder", mode, path};
                const auto override = parse_application_options(selected);
                override.apply_decoder_option();
                require(std::string(std::getenv("LUMAFRAMEVR_DECODER")) == mode, "CLI decoder overrides even an invalid environment value");
                auto opened = ::media::open(path);
                require(std::holds_alternative<::media::Session>(opened), "all CLI decoder modes remain playable without a device");
            }
            const std::string_view invalid[]{"--decoder", "invalid", path};
            const std::string_view missing[]{"--decoder"};
            rejects([&] { parse_application_options(invalid); }, "invalid CLI decoder");
            rejects([&] { parse_application_options(missing); }, "missing CLI decoder");
            ScopedDecoderMode environment("software");
            const std::string_view default_arguments[]{path};
            const auto defaults = parse_application_options(default_arguments);
            defaults.apply_decoder_option();
            require(!defaults.decoder && std::string(std::getenv("LUMAFRAMEVR_DECODER")) == "software", "absent CLI option leaves the environment unchanged");
            require(application_help().find("--decoder auto|hardware|software|v4l2") != std::string_view::npos, "help documents the Steam-friendly decoder option");
        }
        {
            // This is part of the existing media case, so CTest's case count stays unchanged.
            ScopedDecoderMode mode("v4l2");
            ScopedDecoderMode absent("/nonexistent/lumaframevr-v4l2-decoder", "LUMAFRAMEVR_V4L2_DEVICE");
            std::FILE* log = std::fopen("v4l2-fallback.log", "w+");
            require(log != nullptr, "open fallback diagnostic capture in the test build directory");
            const int saved = dup(STDERR_FILENO);
            require(saved >= 0, "save stderr for fallback diagnostic");
            std::fflush(stderr);
            require(dup2(fileno(log), STDERR_FILENO) >= 0, "capture fallback diagnostic");
            auto opened = ::media::open(source.input().c_str());
            std::fflush(stderr);
            require(dup2(saved, STDERR_FILENO) >= 0, "restore stderr after fallback startup");
            close(saved);
            std::rewind(log);
            std::string diagnostic;
            char bytes[1024];
            while(const auto count = std::fread(bytes, 1, sizeof(bytes), log)) diagnostic.append(bytes, count);
            std::fclose(log);
            require(std::holds_alternative<::media::Session>(opened), "v4l2 without a device falls back instead of failing open");
            require(diagnostic.find("V4L2") != std::string::npos && diagnostic.find("falling back to FFmpeg") != std::string::npos &&
                            diagnostic.find("/nonexistent/lumaframevr-v4l2-decoder") != std::string::npos && diagnostic.find("open V4L2 decoder") != std::string::npos,
                    "V4L2 fallback logs the absent-device reason and FFmpeg decision");
            auto session = std::move(std::get<::media::Session>(opened));
            require(!session.info().hardware, "V4L2 fallback uses FFmpeg software without retrying the VPU");
            ::media::Frame frame;
            do {
                result = session.next(frame).status;
            } while(result == 2);
            require(result == 1 && frame.get().software, "V4L2 fallback delivers a software frame");
            require(std::equal(original.begin(), original.end(), frame.get().data[0]), "fallback preserves FFmpeg pixels");
            near(frame.get().pts, moved.pts(), 1e-9, "fallback preserves FFmpeg timestamp");
            frame = {};
            int decoded = 0;
            do {
                result = session.next(frame).status;
                decoded += result == 1;
                frame = {};
            } while(result > 0);
            require(result == 0 && decoded > 0, "V4L2 fallback decodes and drains through EOF");
            require(session.seek(0.5).status == 0, "V4L2 fallback remains seekable");
        }
        native_audio_acceptance();
        std::cout << "decode/seek/EOF/restart/retention/native-byte-equivalence/RAII-moves/returned-errors/decoder-policy verified\n";
    });
}
