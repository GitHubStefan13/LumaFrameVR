#include "media_decoder_startup.hpp"
#include "media_codec.hpp"
#include "media_v4l2.hpp"
#include <array>
#include <cstdlib>
#include <string_view>
#include <utility>
extern "C" {
#include <libavutil/opt.h>
}

namespace {
class SessionDecoderPublication {
   public:
    explicit SessionDecoderPublication(Media& media) : media_(media), context_(media.decoder) {}
    ~SessionDecoderPublication() { media_.decoder = context_.release(); }
    SessionDecoderPublication(const SessionDecoderPublication&) = delete;
    SessionDecoderPublication& operator=(const SessionDecoderPublication&) = delete;
    void clear() {
        context_.reset();
        media_.decoder = nullptr;
    }

   private:
    Media& media_;
    media::CodecContext context_;
};
enum class DecoderMode { Auto, Hardware, Software };

// Opt-in only: installing get_format can bypass FFmpeg's own DRM_PRIME negotiation.
AVPixelFormat choose_dma_buf_format(AVCodecContext*, const AVPixelFormat* formats) {
    for(const AVPixelFormat* format = formats; *format != AV_PIX_FMT_NONE; ++format)
        if(*format == AV_PIX_FMT_DRM_PRIME)
            return *format;
    return formats[0];
}
}  // namespace

void media::CodecDeleter::operator()(AVCodecContext* context) const noexcept {
    avcodec_free_context(&context);
}
int media::open_decoder(Media& value) {
    Media* media = &value;
    AVStream* stream = media->file->streams[media->stream];
    const char* hardware = decoder_name(stream->codecpar);
    const char* forced = std::getenv("LUMAFRAMEVR_DECODER");
    const std::string_view requested_mode = forced ? forced : "auto";
    const DecoderMode mode = requested_mode == "hardware" ? DecoderMode::Hardware : requested_mode == "software" ? DecoderMode::Software : DecoderMode::Auto;
    if(requested_mode == "v4l2" && !media->skip_hardware_decoder)
        return open_v4l2_decoder(value);
    if(forced && requested_mode != "hardware" && requested_mode != "software" && requested_mode != "auto" && requested_mode != "v4l2") {
        fprintf(stderr, "Decoder: invalid LUMAFRAMEVR_DECODER=%s (expected auto/hardware/software)\n", forced);
        return AVERROR(EINVAL);
    }
    if(media->skip_hardware_decoder || mode == DecoderMode::Software || (media_vpu_written_off() && mode != DecoderMode::Hardware)) {
        if(!media->skip_hardware_decoder && !media_vpu_written_off())
            fprintf(stderr, "Decoder: software forced by LUMAFRAMEVR_DECODER\n");
        if(media_vpu_written_off() && !media->skip_hardware_decoder && !(forced && requested_mode == "software"))
            fprintf(stderr, "Decoder: skipping the VPU (written off by an earlier file)\n");
        media->skip_hardware_decoder = 1;
        return open_software_decoder(value);
    }
    // Auto starts with our own V4L2 queue: it is the path that hands back DRM PRIME frames, so the
    // default does not route through FFmpeg's v4l2m2m hwaccel. Only a stream the queue can describe
    // takes this route; everything else - an unsupported codec, depth or chroma layout, a written-off
    // VPU, an absent device - falls through to the same software decoder the FFmpeg path falls back
    // to. --decoder hardware still means FFmpeg's own hwaccel.
    if(mode == DecoderMode::Auto && v4l2_accepts(*stream->codecpar))
        return open_v4l2_decoder(value);
    const AVCodec* codec = hardware ? avcodec_find_decoder_by_name(hardware) : nullptr;
    if(!codec) {
        if(hardware)
            fprintf(stderr, "Decoder: %s is not in this build; using software\n", hardware);
        return open_software_decoder(value);
    }
    media->software_decoding = 0;
    media->first_frame_observed = 0;
    media->decoder_generation++;
    media->decoder = avcodec_alloc_context3(codec);
    {
        SessionDecoderPublication decoder(value);
        if(!media->decoder)
            return AVERROR(ENOMEM);
        int result = avcodec_parameters_to_context(media->decoder, stream->codecpar);
        if(result >= 0) {
            const char* requested = std::getenv("LUMAFRAMEVR_CAPTURE_BUFFERS");
            int capture_buffers = requested ? std::atoi(requested) : 4;
            if(capture_buffers > 0) {
                int option = av_opt_set_int(media->decoder->priv_data, "num_capture_buffers", capture_buffers, 0);
                if(option < 0 && option != AVERROR_OPTION_NOT_FOUND)
                    result = option;
            }
        }
        if(result >= 0) {
            int option = av_opt_set_int(media->decoder->priv_data, "drm_prime", 1, 0);
            if(option == AVERROR_OPTION_NOT_FOUND)
                fprintf(stderr, "Decoder: this FFmpeg build has no DRM_PRIME support; the VPU will hand back CPU frames\n");
            else if(option < 0)
                result = option;
        }
        if(std::getenv("LUMAFRAMEVR_DRM_PRIME_FORMAT"))
            media->decoder->get_format = choose_dma_buf_format;
        media->decoder->apply_cropping = 0;
        media->decoder->pkt_timebase = stream->time_base;
        media->decoder->framerate = av_guess_frame_rate(media->file, stream, nullptr);
        if(result >= 0)
            result = avcodec_open2(media->decoder, codec, nullptr);
        if(result >= 0)
            return 0;
        std::array<char, AV_ERROR_MAX_STRING_SIZE> detail{};
        av_strerror(result, detail.data(), detail.size());
        fprintf(stderr, "Decoder: %s unavailable (%s) for %dx%d, falling back to software\n", codec->name, detail.data(), stream->codecpar->width, stream->codecpar->height);
        decoder.clear();
    }
    media->skip_hardware_decoder = 1;
    media_vpu_write_off();
    return open_software_decoder(value);
}
int media::open_software_decoder(Media& value) {
    Media* media = &value;
    media->v4l2.reset();
    AVStream* stream = media->file->streams[media->stream];
    const AVCodec* codec = software_decoder(*stream->codecpar);
    if(!codec)
        return AVERROR_DECODER_NOT_FOUND;
    CodecContext previous(std::exchange(media->decoder, nullptr));
    previous.reset();
    media->decoder_generation++;
    media->software_decoding = 1;
    media->first_frame_observed = 0;
    media->decoder = avcodec_alloc_context3(codec);
    SessionDecoderPublication decoder(value);
    if(!media->decoder)
        return AVERROR(ENOMEM);
    int result = avcodec_parameters_to_context(media->decoder, stream->codecpar);
    if(result < 0)
        return result;
    media->decoder->apply_cropping = 0;
    media->decoder->pkt_timebase = stream->time_base;
    media->decoder->framerate = av_guess_frame_rate(media->file, stream, nullptr);
    media->decoder->thread_count = software_decoder_thread_count();
    media->decoder->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    result = avcodec_open2(media->decoder, codec, nullptr);
    if(result < 0)
        return result;
    media->flushing_at_end = 0;
    media->info = MediaInfo{};
    media_fill_info(media);
    return 0;
}
int media_open_decoder(Media* media) {
    return media::open_decoder(*media);
}
int open_software_decoder(Media* media) {
    return media::open_software_decoder(*media);
}
