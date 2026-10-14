#pragma once
#include "media_internal.h"
#include "audio_output.h"
#ifdef __cplusplus
extern "C" {
#endif
#include <libswresample/swresample.h>

#ifdef __cplusplus
}
#endif

struct Audio {
#ifdef __cplusplus
    Audio() = default;
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;
    int convert_to_playback_format(AVFrame* input, double pts);
    int decode(Media& media);
#endif
    AVCodecContext* decoder;
    SwrContext* resampler;
    AVFrame *frame, *pcm;
    AVPacket* packet;
    AudioOutput* output;
    int flushing_at_end, source_ended, offset, seeking, input_rate;
    int64_t written, frame_start;
    double target, next_pts, end, time_base;
};

#ifdef __cplusplus
extern "C" {
#endif
int audio_decode(Media* media);
#ifdef __cplusplus
}
#endif
