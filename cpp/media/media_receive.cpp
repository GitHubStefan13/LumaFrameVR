#include "media_result.hpp"
#include <exception>
extern "C" {
#include <libavutil/time.h>
}

static constexpr int64_t FIRST_FRAME_BUDGET_US = 4000000;

static int64_t first_frame_budget_us(const Media* media) {
    (void)media;
    const char* value = getenv("LUMAFRAMEVR_DECODE_STALL_MS");
    if(!value)
        return FIRST_FRAME_BUDGET_US;
    long ms = strtol(value, NULL, 10);
    if(ms <= 0)
        return FIRST_FRAME_BUDGET_US;
    return (int64_t)ms * 1000;
}

static const char* stall_detail(int result) {
    if(result == AVERROR(EAGAIN))
        return "the decoder produced no frame (EAGAIN)";
    if(result == AVERROR(ENOMEM))
        return "the VPU reported it is out of memory";
    if(result < 0)
        return "the decoder returned an error";
    return "the decoder produced no frame";
}

media::Result media::receive(Media& state) {
    Media* media = &state;
    if(media->v4l2) {
        try {
            return media->v4l2->receive(state);
        } catch(const std::exception& error) {
            const int switched = fallback_v4l2(state, error.what(), true);
            if(switched < 0) return failure("switch V4L2 to FFmpeg", switched);
        }
    }
    av_frame_unref(media->frame);
    for(;;) {
        int64_t started = media_trace_start(media);
        int result = avcodec_receive_frame(media->decoder, media->frame);
        media_trace_end(started, &media->trace.receive_us);
        if(media->trace_enabled) {
            media->trace.receive_calls++;
            media->trace.again += result == AVERROR(EAGAIN);
            media->trace.frames += result == 0;
        }
        if(result == AVERROR_EOF)
            return {0, {}};
        if(result == 0) {
            media->stall_started_us = 0;
            break;
        }
        if(result != AVERROR(EAGAIN)) {
            // The VPU gave up on this stream. Software decoding is slower but it plays, so
            // switch and keep going instead of ending playback here. Only this decoder is
            // written off: one stream the VPU chokes on must not demote every later file.
            if(!media->software_decoding) {
                fprintf(stderr, "Decoder: the VPU returned %s; switching to software\n", stall_detail(result));
                media->skip_hardware_decoder = 1;
                if(open_software_decoder(media) >= 0)
                    continue;
            }
            return failure("Iris receive frame", result);
        }

        if(!media->software_decoding && !media->stall_started_us)
            media->stall_started_us = av_gettime_relative();

        if(!media->software_decoding && media->stall_started_us && !media->flushing_at_end && !media->preview_requested &&
           av_gettime_relative() - media->stall_started_us > first_frame_budget_us(media)) {
            fprintf(stderr, "Decoder: hardware decoder produced no frame in %lld ms (%s)\n", (long long)(first_frame_budget_us(media) / 1000), stall_detail(result));
            media->skip_hardware_decoder = 1;
            media_vpu_write_off();
            int switched = open_software_decoder(media);
            if(switched >= 0)
                fprintf(stderr, "Decoder: switched to software: %s\n", media->decoder->codec->name);
            if(switched < 0)
                return failure("switch to the software decoder", switched);
            continue;
        }
        const auto changed = media::reconfigure(*media, false);
        if(changed.status < 0)
            return changed;
        if(media->flushing_at_end || changed.status)
            return {2, {}};
        result = media->packet->size ? 0 : media_read_packet(media);

        if(result == AVERROR(EAGAIN) || result == AVERROR(ENOBUFS))
            return {2, {}};
        if(result < 0 && result != AVERROR_EOF)
            return failure("read file", result);
        media->flushing_at_end = result == AVERROR_EOF;
        started = media_trace_start(media);
        result = avcodec_send_packet(media->decoder, media->flushing_at_end ? NULL : media->packet);
        media_trace_end(started, &media->trace.send_us);
        if(media->trace_enabled) {
            media->trace.send_calls++;
            media->trace.again += result == AVERROR(EAGAIN);
        }
        if(result == AVERROR(EAGAIN))
            return {2, {}};
        av_packet_unref(media->packet);
        if(result < 0)
            return failure("Iris send packet", result);
    }
    return {1, {}};
}

int receive_decoded_frame(Media* media, char* error, size_t capacity) {
    try {
        return media::export_result(media::receive(*media), error, capacity);
    } catch(const std::bad_alloc&) {
        return media_error(error, capacity, "Iris receive frame", AVERROR(ENOMEM));
    }
}
