#include "media_description.hpp"
#include "media_session.hpp"
#include <array>
#include <cstdio>

namespace {
const char* channel_name(const AVChannelLayout* layout) {
    std::array<char, 64> buffer{};
    if(!av_channel_layout_describe(layout, buffer.data(), buffer.size()))
        return "unknown";
    static thread_local std::array<char, 64> stored{};
    std::snprintf(stored.data(), stored.size(), "%s", buffer.data());
    return stored.data();
}
}  // namespace

media::MediaInfo media::describe(Media& value) {
    const Media* media = &value;
    MediaInfo description{};
    MediaInfo* out = &description;
    if(!media->file || media->stream < 0 || static_cast<unsigned>(media->stream) >= media->file->nb_streams)
        return description;
    AVStream* stream = media->file->streams[media->stream];
    const AVCodecParameters* p = stream->codecpar;
    const char* name = media->v4l2 ? "v4l2" : media->decoder ? media->decoder->codec->name : decoder_name(p);
    if(!name)
        return description;
    std::snprintf(out->name, sizeof(out->name), "%s", name);
    out->hardware = !media->software_decoding;
    out->width = media->decoder ? media->decoder->width : p->width;
    out->height = media->decoder ? media->decoder->height : p->height;
    out->bits = media->decoder ? media->decoder->bits_per_raw_sample : 0;
    if(!out->bits) {
        const AVPixFmtDescriptor* pixel = av_pix_fmt_desc_get(media->decoder ? media->decoder->pix_fmt : static_cast<AVPixelFormat>(p->format));
        if(!pixel || !pixel->comp[0].depth)
            pixel = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(p->format));
        out->bits = pixel ? pixel->comp[0].depth : 0;
    }
    AVRational rate = av_guess_frame_rate(media->file, stream, nullptr);
    out->fps = rate.den ? av_q2d(rate) : (media->step > 0 ? 1.0 / media->step : 0.0);
    out->bitrate_mbps = media->file->bit_rate > 0 ? media->file->bit_rate / 1e6 : 0.0;
    return description;
}

void media_fill_info(Media* media) {
    media->info = media::describe(*media);
}
media::MediaInfo media::info(Media& media) {
    if(!media.info.name[0])
        media_fill_info(&media);
    return media.info;
}
media::MediaInfo media::Session::info() const {
    return get() ? media::info(*get()) : MediaInfo{};
}

void media_report(Media* media) {
    if(!media || !media->file)
        return;
    media_fill_info(media);
    AVStream* stream = media->file->streams[media->stream];
    const AVCodecParameters* p = stream->codecpar;
    const AVPixFmtDescriptor* pixel = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(p->format));
    const char* bits = pixel ? (pixel->comp[0].depth == 10 ? "10-bit" : "8-bit") : "unknown";
    AVRational rate = av_guess_frame_rate(media->file, stream, nullptr);
    double fps = rate.den ? av_q2d(rate) : 1.0 / media->step;
    fprintf(stderr, "Video: %s %dx%d %s %.3f fps",
            p->codec_id == AV_CODEC_ID_H264   ? "H.264"
            : p->codec_id == AV_CODEC_ID_HEVC ? "HEVC"
            : p->codec_id == AV_CODEC_ID_VP9  ? "VP9"
                                              : avcodec_get_name(p->codec_id),
            p->width, p->height, bits, fps);
    if(pixel)
        fprintf(stderr, " (%s) chroma=%s", pixel->name, pixel->log2_chroma_w == 1 && pixel->log2_chroma_h == 1 ? "4:2:0" : "other");
    fprintf(stderr, "\n");
    {
        const char* profile = avcodec_profile_name(p->codec_id, p->profile);
        if(profile)
            fprintf(stderr, "Video profile: %s\n", profile);
    }
    if(media->file->bit_rate > 0)
        fprintf(stderr, "Bitrate: %.2f Mbit/s\n", media->file->bit_rate / 1e6);
    if(stream->avg_frame_rate.num && stream->avg_frame_rate.den)
        fprintf(stderr, "Video average rate: %.3f fps\n", av_q2d(stream->avg_frame_rate));
    if(media->duration > 0)
        fprintf(stderr, "Duration: %.3f s\n", media->duration);
    int audio = av_find_best_stream(media->file, AVMEDIA_TYPE_AUDIO, -1, media->stream, nullptr, 0);
    if(audio >= 0) {
        const AVCodecParameters* a = media->file->streams[audio]->codecpar;
        fprintf(stderr, "Audio: %s, codec_id=%d, %d Hz, %d ch (%s)\n", avcodec_get_name(a->codec_id), a->codec_id, a->sample_rate, a->ch_layout.nb_channels, channel_name(&a->ch_layout));
    } else {
        fprintf(stderr, "Audio: none\n");
    }
    fprintf(stderr, "Hardware decoding: %s\n", media->software_decoding ? "INACTIVE (software compatibility/thumbnail path)" : "SELECTED (/dev/video* V4L2 M2M; first output frame still required)");
    fprintf(stderr, "Decoder policy: hardware first, software whenever the VPU cannot deliver\n");
    if(!media->software_decoding)
        fprintf(stderr, "DMA-BUF export: requested (drm_prime=1), not yet proven by a frame\n");
}
