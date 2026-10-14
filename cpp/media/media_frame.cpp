#include "media_result.hpp"
#include "media_owned_frame.hpp"
#include <new>
extern "C" {
#include <libavutil/opt.h>
}
static media::Result next_frame(Media& state, media::FrameData& value) {
    Media* media = &state;
    auto* out = &value;
    char error[4096]{};
    const auto capacity = sizeof(error);
    if(!media->preview_repeat_requested) {
        auto result = media::receive(*media);
        if(result.status != 1)
            return result;
    }
    media->preview_repeat_requested = 0;
    AVFrame* frame = media->frame;
    if(media->seek_target >= 0 && frame->best_effort_timestamp == AV_NOPTS_VALUE)
        return media::failure("seek requires frame timestamps", AVERROR_INVALIDDATA);
    double pts = frame->best_effort_timestamp == AV_NOPTS_VALUE ? media->fallback_pts : frame->best_effort_timestamp * media->time_base - media->start;
    media->fallback_pts = pts + media->step;
    // A seek restarts the decoder at a keyframe before the requested position,
    // so every frame that precedes the target must be discarded rather than
    // presented - for media with audio as much as without it. The seek's own
    // preview request does not excuse them: presenting a pre-target frame is
    // what drops the picture into the wrong frame while the audio, already
    // reset to the target, starts on time. The audio side needs nothing here.
    int before = media->seek_target >= 0 && pts < media->seek_target;
    if(before)
        return {2, {}};

    // The hardware decoder normally returns a DMA-BUF frame. Some V4L2 drivers
    // ignore drm_prime and hand back a CPU frame instead; that frame is still
    // valid, so accept it as a CPU frame rather than failing the whole playback.
    int result;
    if(media->software_decoding) {
        result = validate_software_frame(frame, error, capacity);
    } else if(frame->format == AV_PIX_FMT_DRM_PRIME) {
        result = validate_dma_buf_frame(frame, error, capacity);
    } else {
        result = validate_software_frame(frame, error, capacity);
        if(result >= 0) {
            if(!media->cpu_frame_reported) {
                media->cpu_frame_reported = 1;
                fprintf(stderr,
                        "Decoder: the VPU returned a CPU frame (%s), not DMA-BUF; "
                        "using the CPU upload path\n",
                        av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format)));
            }
            media->cpu_frames = 1;
        }
    }
    if(result < 0) {
        if(media->trace_enabled)
            media->trace.rejected++;
        if(media->v4l2) {
            const int switched = media::fallback_v4l2(*media, error, true);
            return switched < 0 ? media::failure("switch V4L2 to FFmpeg", switched) : media::Result{2, {}};
        }
        return before ? media::Result{2, {}} : media::Result{result, error};
    }
    int preview = media->preview_requested;
    media->preview_requested = 0;
    media->preview_repeat_requested = preview && !before;
    if(!preview && !before)
        media->seek_target = -1;
    AVFrame* owned = av_frame_clone(frame);
    if(!owned)
        return media::failure("retain frame", AVERROR(ENOMEM));
    FrameOwner* owner = new(std::nothrow) FrameOwner{};
    if(!owner) {
        av_frame_free(&owned);
        return media::failure("allocate frame owner", AVERROR(ENOMEM));
    }
    *owner = (FrameOwner){owned, media};
    atomic_fetch_add(&media->references, 1);
    *out = {};
    out->owner = owner;
    out->width = static_cast<int>(owned->width - owned->crop_left - owned->crop_right);
    out->height = static_cast<int>(owned->height - owned->crop_top - owned->crop_bottom);
    out->colorspace = owned->colorspace == AVCOL_SPC_UNSPECIFIED ? media->decoder->colorspace : owned->colorspace;
    out->full_range = owned->color_range == AVCOL_RANGE_UNSPECIFIED ? media->decoder->color_range : owned->color_range;
    out->color = read_frame_color(owned, media);
    out->sample_aspect_ratio = media_sample_aspect_ratio(media, owned);
    out->pts = pts;

    // The descriptor must match the frame that was actually validated: a CPU
    // frame (software decoder, or a VPU that ignored drm_prime) uses the software
    // descriptor, only a real DRM PRIME frame uses the DMA-BUF descriptor.
    if(owned->format == AV_PIX_FMT_DRM_PRIME && !media->software_decoding)
        describe_frame(out, owned, media);
    else
        fill_software_descriptor(out, owned);
    if(!media->first_frame_observed) {
        media->first_frame_observed = 1;
        media->info.bits = out->color.bits;
        const char* fmt = av_get_pix_fmt_name(static_cast<AVPixelFormat>(owned->format));
        fprintf(stderr, "Decoded frame: decoder=%s hardware_frames=%s pixel_format=%s depth=%d transfer=%d primaries=%d matrix=%d range=%d chroma_location=%d\n",
                media->v4l2 ? "v4l2" : media->decoder->codec->name, out->software ? "no" : "yes (DRM PRIME)", fmt ? fmt : "unknown", out->color.bits, out->color.transfer, out->color.primaries,
                out->colorspace, out->full_range, out->color.chroma_location);
        fprintf(stderr, "HDR metadata: mastering_min=%.4f mastering_max=%.1f MaxCLL=%.1f MaxFALL=%.1f nits\n", out->color.mastering_min_nits, out->color.mastering_max_nits, out->color.max_cll,
                out->color.max_fall);
        if(!out->software) {
            uint32_t format = out->dmabuf.format;
            fprintf(stderr, "DMA-BUF: yes fd=%d DRM=%c%c%c%c modifier=%#llx offsets=%llu,%llu pitches=%llu,%llu; CPU frame copy=no CPU colorspace conversion=no\n", out->dmabuf.fd, format & 255,
                    (format >> 8) & 255, (format >> 16) & 255, (format >> 24) & 255, (unsigned long long)out->dmabuf.modifier, (unsigned long long)out->dmabuf.offsets[0],
                    (unsigned long long)out->dmabuf.offsets[1], (unsigned long long)out->dmabuf.pitches[0], (unsigned long long)out->dmabuf.pitches[1]);
        } else {
            fprintf(stderr, "DMA-BUF: no; software compatibility mode requires CPU YUV packing and GPU upload (not an 8K60 path)\n");
        }
    }
    return {preview ? 3 : 1, {}};
}

void media::release(FrameData& value) noexcept {
    auto* frame = &value;
    FrameOwner* owner = static_cast<FrameOwner*>(frame->owner);
    av_frame_free(&owner->frame);
    media::unref(owner->media);
    delete owner;
    frame->owner = NULL;
}

media::Result media::reconfigure(Media& state, bool released) {
    Media* media = &state;

    if(media->software_decoding || media->v4l2)
        return {0, {}};
    int64_t pending = 0;
    // capture_changed is a V4L2-specific option. A decoder that does not expose
    // it (a different V4L2 build) reports AVERROR_OPTION_NOT_FOUND, which means
    // there is no pending capture change, not that the query failed.
    int result = av_opt_get_int(media->decoder->priv_data, "capture_changed", 0, &pending);
    if(result == AVERROR_OPTION_NOT_FOUND)
        return {0, {}};
    if(result < 0)
        return failure("query capture change", result);
    if(!released || pending != 1)
        return {pending == 1, {}};
    if(atomic_load(&media->references) != 1)
        return failure("release frames before reconfiguration", AVERROR(EBUSY));
    av_frame_unref(media->frame);
    media->preview_requested |= media->preview_repeat_requested;
    media->preview_repeat_requested = 0;
    result = av_opt_set_int(media->decoder->priv_data, "capture_changed", 2, 0);
    if(result == AVERROR_OPTION_NOT_FOUND)
        return {0, {}};
    if(result < 0)
        return failure("acknowledge capture change", result);
    media->decoder_generation++;
    return {0, {}};
}

media::Result media::Session::next(Frame& frame) {
    return next_frame(*get(), frame.get());
}
media::Result media::Session::reconfigure(bool released) {
    return media::reconfigure(*get(), released);
}
