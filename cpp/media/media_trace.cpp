#include "audio_internal.h"
#include "media_session.hpp"
extern "C" {
#include <libavutil/time.h>
}

static void profile_session(Media* media, bool continuous) {
    media->profile_enabled = 1;
    if(continuous) {
        media->continuous_trace = media->trace_enabled = 1;
        if(media->audio)
            media->audio->output->trace = &media->trace;
    }
    media_reader_trace(media, 0);
}

static MediaTrace trace_session(Media* media) {
    media->trace_enabled = media->continuous_trace;
    MediaTrace trace = media->trace;
    MediaTrace reader = media_reader_trace(media, 0);
    trace.read_us = reader.read_us;
    trace.packets = reader.packets;
    trace.packet_bytes = reader.packet_bytes;
    return trace;
}

void media::Session::profile(bool continuous) {
    profile_session(get(), continuous);
}
MediaTrace media::Session::trace() {
    return trace_session(get());
}

int64_t media_trace_start(Media* media) {
    return media->trace_enabled ? av_gettime_relative() : 0;
}

void media_trace_end(int64_t started, int64_t* elapsed) {
    if(started)
        *elapsed += av_gettime_relative() - started;
}
