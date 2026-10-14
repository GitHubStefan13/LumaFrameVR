#include "media_session.hpp"
#include "media_result.hpp"
#include "media_errors.hpp"
#include "media_codec.hpp"
#include "media_decoder_startup.hpp"
#include <cstring>

Media::~Media() {
    media_reader_close(this);
    audio_close(audio);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&decoder);
    avformat_close_input(&file);
}

void media::unref(Media* media) noexcept {
    if(!media || media->references.fetch_sub(1) != 1)
        return;
    std::unique_ptr<Media> owner(media);
}

namespace media {
void SessionDeleter::operator()(Media* media) const noexcept {
    media::stop(*media);
    media::unref(media);
}
static int interrupted(void* handle) {
    return static_cast<Media*>(handle)->interrupted.load();
}

OpenResult open(const char* path, bool software_only) {
    std::unique_ptr<Media> allocated;
    try {
        allocated = std::make_unique<Media>();
    } catch(const std::bad_alloc&) {
        return Error{AVERROR(ENOMEM), failure("allocate decoder", AVERROR(ENOMEM)).error};
    }
    allocated->references = 1;
    allocated->interrupted = 0;
    Session session(allocated.release());
    Media* media = session.get();
    media->audio_stream = -1;
    media->skip_hardware_decoder = software_only;
    auto failed = [&](int result) -> OpenResult {
        if(media->file && media->stream >= 0 && static_cast<unsigned>(media->stream) < media->file->nb_streams)
            return Error{result, startup_failure(media->file->streams[media->stream]->codecpar, path, "open native decoder", result).error};
        auto message = legacy_error(
                [&](char* error, size_t capacity) {
                    char detail[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(result, detail, sizeof(detail));
                    snprintf(error, capacity, "open native decoder: %s (%d); file=%s", detail, result, path);
                },
                std::strlen(path));
        return Error{result, std::move(message)};
    };
    media->file = avformat_alloc_context();
    if(!media->file)
        return failed(AVERROR(ENOMEM));
    media->file->interrupt_callback = AVIOInterruptCB{interrupted, media};
    int result = video_stream(media->file, path);
    if(result < 0)
        return failed(result);
    media->stream = result;
    AVStream* stream = media->file->streams[media->stream];
    AVCodecParameters* p = stream->codecpar;
    if(!decodable(*p)) {
        return Error{AVERROR_INVALIDDATA, unsupported_video(p, path).error};
    }
    // 10-bit is refused rather than played: the software path packs 8-bit YUV, and the device says the
    // picture is not right. The check reads the depth the parser reported and, where that is absent,
    // the pixel format by name - both facts about the stream, neither needing the device, and neither
    // needing av_pix_fmt_desc_get, whose absence from the link line is an ordering matter rather than
    // a missing symbol. It comes out when the V4L2 decoder carries P010 and has been compared on the
    // frame.
    const auto ten_bit_format = [](AVPixelFormat format) {
        switch(format) {
        case AV_PIX_FMT_YUV420P10LE: case AV_PIX_FMT_YUV420P10BE:
        case AV_PIX_FMT_YUV422P10LE: case AV_PIX_FMT_YUV422P10BE:
        case AV_PIX_FMT_YUV444P10LE: case AV_PIX_FMT_YUV444P10BE:
        case AV_PIX_FMT_P010LE: case AV_PIX_FMT_P010BE:
            return true;
        default:
            return false;
        }
    };
    // The ten-bit refusal guards the software path, which packs 8-bit YUV. The V4L2 decoder carries
    // Q10C and P010 directly, so asking for it must not be refused here - and asking for it is exactly
    // what a ten-bit file needs. The request is read from the environment, which is where the decoder
    // option is recorded before the workers start.
    const char* requested_decoder = std::getenv("LUMAFRAMEVR_DECODER");
    const bool v4l2_requested = requested_decoder && std::string_view(requested_decoder) == "v4l2";
    if(!v4l2_requested &&
       (p->bits_per_raw_sample > 8 || ten_bit_format(static_cast<AVPixelFormat>(p->format)))) {
        return Error{AVERROR_INVALIDDATA, unsupported_video(p, path).error};
    }
    media->packet = av_packet_alloc();
    media->frame = av_frame_alloc();
    if(!media->packet || !media->frame)
        return failed(AVERROR(ENOMEM));
    media->time_base = av_q2d(stream->time_base);
    media->step = av_q2d(av_inv_q(av_guess_frame_rate(media->file, stream, nullptr)));
    if(media->step <= 0)
        media->step = 1.0 / 30.0;
    media->start = stream->start_time == AV_NOPTS_VALUE ? 0 : stream->start_time * media->time_base;
    media->seek_target = -1;
    if(stream->duration != AV_NOPTS_VALUE && stream->duration > 0)
        media->duration = stream->duration * media->time_base;
    else if(media->file->duration != AV_NOPTS_VALUE && media->file->duration > 0)
        media->duration = media->file->duration / static_cast<double>(AV_TIME_BASE);
    result = open_decoder(*media);
    if(result < 0)
        return failed(result);
    fprintf(stderr, "Decoder: %s, %dx%d, %.3f fps; %s\n", media->v4l2 ? "v4l2" : media->decoder->codec->name, media->decoder->width, media->decoder->height, 1.0 / media->step,
            media->software_decoding ? "software (VPU declined)" : "hardware (VPU)");
    media_read_metadata(media);
    media_report(media);
    return session;
}
}  // namespace media
