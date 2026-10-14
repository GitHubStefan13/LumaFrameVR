#include "media_result.hpp"
#include <cmath>

int media_seek_file(Media* media, int64_t target) {
    AVStream* stream = media->file->streams[media->stream];
    int64_t start = stream->start_time == AV_NOPTS_VALUE ? 0 : stream->start_time;
    int64_t preroll = av_rescale_q((int64_t)ceil(audio_preroll(media->audio) * AV_TIME_BASE), AV_TIME_BASE_Q, stream->time_base);
    int64_t before = target - preroll < start ? start : target - preroll;
    for(;;) {
        media_clear_packets(media, target);
        int64_t started = media_trace_start(media);
        int result = av_seek_frame(media->file, media->stream, before, AVSEEK_FLAG_BACKWARD);
        media_trace_end(started, &media->trace.seek_us);
        if(media->trace_enabled)
            media->trace.seek_calls++;
        if(result < 0)
            return result;
        do {
            result = media_read_packet(media);
        } while(result == AVERROR(EAGAIN));
        if(result < 0)
            return result == AVERROR_EOF ? 0 : result;
        if(media->packet->pts == AV_NOPTS_VALUE)
            return AVERROR_INVALIDDATA;
        if(media->packet->pts <= target)
            return 0;
        if(media->packet->dts == AV_NOPTS_VALUE || media->packet->dts > before)
            return AVERROR_INVALIDDATA;
        before = media->packet->dts - 1;
    }
}

static int can_continue_to(Media* media, int64_t target) {
    int64_t current = media->frame->best_effort_timestamp;
    if(media->flushing_at_end || current == AV_NOPTS_VALUE || current >= target || media->frame->decode_error_flags || media->frame->flags & AV_FRAME_FLAG_CORRUPT)
        return 0;
    AVStream* stream = media->file->streams[media->stream];
    int index = av_index_search_timestamp(stream, current, AVSEEK_FLAG_BACKWARD);
    return index >= 0 && index == av_index_search_timestamp(stream, target, AVSEEK_FLAG_BACKWARD);
}

static media::Result seek_to(Media& state, double seconds) {
    Media* media = &state;
    if(!media->continuous_trace)
        media->trace = {};
    media->trace_enabled = media->profile_enabled;
    media_reader_trace(media, !media->continuous_trace);
    if(!std::isfinite(seconds) || seconds < 0 || seconds >= (double)(INT64_MAX / AV_TIME_BASE))
        return media::failure("invalid seek target", AVERROR(EINVAL));
    if(media->references.load() != 1)
        return media::failure("release decoded frames before seeking", AVERROR(EBUSY));
    AVStream* stream = media->file->streams[media->stream];
    double duration = media->duration;
    if(duration > 0 && seconds > duration)
        seconds = duration;
    int64_t offset = stream->start_time == AV_NOPTS_VALUE ? 0 : stream->start_time;
    int64_t timestamp = av_rescale_q((int64_t)(seconds * AV_TIME_BASE), AV_TIME_BASE_Q, stream->time_base);
    if(timestamp < 0 || (offset > 0 && timestamp > INT64_MAX - offset))
        return media::failure("seek timestamp overflow", AVERROR(EINVAL));
    media->preview_requested = 1;
    media->preview_repeat_requested = 0;
    if(!media->audio && can_continue_to(media, timestamp + offset)) {
        media->seek_target = seconds;
        return {0, {}};
    }
    av_frame_unref(media->frame);
    av_packet_unref(media->packet);
    int64_t started = media_trace_start(media);
    int result = audio_reset(media->audio, seconds);
    media_trace_end(started, &media->trace.audio_reset_us);
    if(result < 0)
        // A slow or unreachable audio device is a hiccup for the sound, not a reason
        // to stop the video. Report it and carry on with the seek.
        fprintf(stderr, "Audio: reset failed (%d); continuing without an audio flush\n", result);
    result = media_seek_file(media, timestamp + offset);
    if(result < 0)
        return media::failure("seek video", result);
    started = media_trace_start(media);
    if(media->v4l2) {
        // Keep the device session and both allocations: a seek flushes the queues
        // and restarts them, so it never re-requests buffers. A capture pool is
        // re-negotiated only when the driver reports a new geometry for the
        // sought segment, which the queue handles on its own.
        result = media::reset_v4l2_decoder(*media);
    } else {
        avcodec_free_context(&media->decoder);
        media_trace_end(started, &media->trace.close_us);
        started = media_trace_start(media);
        result = media_open_decoder(media);
    }
    media_trace_end(started, &media->trace.open_us);
    if(result < 0)
        return media::failure("reopen Iris decoder", result);
    media->flushing_at_end = 0;

    media->stall_started_us = 0;
    media->fallback_pts = media->seek_target = seconds;
    return {0, {}};
}

class ReaderPause {
   public:
    explicit ReaderPause(Media& media) : media_(media) { media_reader_pause(&media_, 1); }
    ~ReaderPause() { media_reader_pause(&media_, 0); }
    ReaderPause(const ReaderPause&) = delete;
    ReaderPause& operator=(const ReaderPause&) = delete;

   private:
    Media& media_;
};

static media::Result seek_session(Media& media, double seconds) {
    int result = media_reader_start(&media);
    if(result < 0)
        return media::failure("start packet reader", result);
    ReaderPause pause(media);
    return seek_to(media, seconds);
}
media::Result media::Session::seek(double seconds) {
    return seek_session(*get(), seconds);
}
