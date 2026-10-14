#include "audio_internal.h"
#include <cmath>

int Audio::convert_to_playback_format(AVFrame* input, double pts) {
    auto* audio = this;
    av_frame_unref(audio->pcm);
    audio->pcm->format = AV_SAMPLE_FMT_FLT;
    audio->pcm->sample_rate = AUDIO_RATE;
    const AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    audio->pcm->ch_layout = stereo;
    if(!audio->resampler) {
        if(!input) {
            audio->source_ended = 1;
            return 1;
        }
        audio->resampler = swr_alloc();
        if(!audio->resampler)
            return AVERROR(ENOMEM);
        int result = swr_config_frame(audio->resampler, audio->pcm, input);
        if(result < 0)
            return result;
        result = swr_init(audio->resampler);
        if(result < 0)
            return result;
        audio->input_rate = input->sample_rate;
    }
    int64_t base = static_cast<int64_t>(audio->input_rate) * AUDIO_RATE;
    double delay = swr_get_delay(audio->resampler, base) / static_cast<double>(base);
    int count = input ? input->nb_samples : 0;
    int capacity = swr_get_out_samples(audio->resampler, count);
    if(capacity < 0)
        return capacity;
    if(capacity > 2 * AUDIO_RATE)
        return AVERROR(ENOBUFS);
    audio->pcm->nb_samples = capacity;
    int result = swr_convert_frame(audio->resampler, audio->pcm, input);
    if(result < 0)
        return result;
    double start = (pts - delay - audio->target) * AUDIO_RATE;
    if(!std::isfinite(start) || start <= INT64_MIN || start >= static_cast<double>(INT64_MAX))
        return AVERROR_INVALIDDATA;
    audio->frame_start = std::llround(start);
    double remaining = (audio->end - audio->target) * AUDIO_RATE - audio->frame_start;
    if(remaining < audio->pcm->nb_samples)
        audio->pcm->nb_samples = static_cast<int>(std::fmax(0, std::round(remaining)));
    audio->offset = 0;
    if(!input && !audio->pcm->nb_samples)
        audio->source_ended = 1;
    return 1;
}

int audio_decode(Media* media) {
    return media->audio->decode(*media);
}

int Audio::decode(Media& state) {
    Media* media = &state;
    auto* audio = this;
    av_frame_unref(audio->frame);
    int result = avcodec_receive_frame(audio->decoder, audio->frame);
    if(result == AVERROR_EOF)
        return convert_to_playback_format(nullptr, audio->next_pts);
    if(result == AVERROR(EAGAIN)) {
        result = media_packet(media, 1, audio->packet);

        if(result == AVERROR(EAGAIN) || result == AVERROR(ENOBUFS))
            return 0;
        if(result < 0 && result != AVERROR_EOF)
            return result;
        if(audio->flushing_at_end)
            return AVERROR_BUG;
        audio->flushing_at_end = result == AVERROR_EOF;
        result = avcodec_send_packet(audio->decoder, audio->flushing_at_end ? nullptr : audio->packet);
        av_packet_unref(audio->packet);
        return result < 0 ? result : 1;
    }
    if(result < 0)
        return result;
    AVFrame* frame = audio->frame;
    if(frame->decode_error_flags || frame->flags & AV_FRAME_FLAG_CORRUPT || frame->sample_rate <= 0)
        return AVERROR_INVALIDDATA;
    if(audio->seeking && frame->best_effort_timestamp == AV_NOPTS_VALUE)
        return AVERROR_INVALIDDATA;
    double pts = frame->best_effort_timestamp == AV_NOPTS_VALUE ? audio->next_pts : frame->best_effort_timestamp * audio->time_base - media->start;
    double tolerance = 2 * audio->time_base + 1.0 / frame->sample_rate;
    if(audio->resampler && std::fabs(pts - audio->next_pts) <= tolerance)
        pts = audio->next_pts;
    audio->next_pts = pts + frame->nb_samples / static_cast<double>(frame->sample_rate);
    if(pts >= audio->target)
        audio->seeking = 0;
    return convert_to_playback_format(frame, pts);
}
