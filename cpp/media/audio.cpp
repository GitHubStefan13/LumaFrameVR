#include "audio_internal.h"
#include <pulse/error.h>
#include <pulse/stream.h>
#include "media_result.hpp"
#include <array>
#include <cmath>
#include <new>

Audio::~Audio() {
    output_close(output);
    swr_free(&resampler);
    av_frame_free(&frame);
    av_frame_free(&pcm);
    av_packet_free(&packet);
    avcodec_free_context(&decoder);
}

void audio_close(Audio* audio) {
    std::unique_ptr<Audio> owner(audio);
}

int audio_open(Media* media) {
    if(media->audio) {
        media->audio_present = 1;
        return 1;
    }
    if(media->reader)
        return AVERROR(EBUSY);
    const AVCodec* codec;
    int result = av_find_best_stream(media->file, AVMEDIA_TYPE_AUDIO, -1, media->stream, &codec, 0);
    if(result == AVERROR_STREAM_NOT_FOUND)
        return 0;
    if(result < 0)
        return result;
    media->audio_stream = result;
    media->audio_present = 1;
    std::unique_ptr<Audio> owner(new(std::nothrow) Audio());
    Audio* audio = media->audio = owner.get();
    if(!audio)
        return AVERROR(ENOMEM);
    audio->decoder = avcodec_alloc_context3(codec);
    audio->frame = av_frame_alloc();
    audio->pcm = av_frame_alloc();
    audio->packet = av_packet_alloc();
    if(!audio->decoder || !audio->frame || !audio->pcm || !audio->packet)
        result = AVERROR(ENOMEM);
    else {
        AVStream* stream = media->file->streams[media->audio_stream];
        audio->time_base = av_q2d(stream->time_base);
        result = avcodec_parameters_to_context(audio->decoder, stream->codecpar);
        if(result >= 0) {
            audio->decoder->pkt_timebase = stream->time_base;
            result = avcodec_open2(audio->decoder, codec, nullptr);
        }
        if(result >= 0) {
            audio->output = output_open();
            if(!audio->output)
                result = AVERROR_EXTERNAL;
        }
        if(result >= 0) {
            audio->next_pts = stream->start_time == AV_NOPTS_VALUE ? 0 : stream->start_time * av_q2d(stream->time_base) - media->start;
            audio->end = stream->duration == AV_NOPTS_VALUE ? INFINITY : audio->next_pts + stream->duration * av_q2d(stream->time_base);
        }
    }
    if(result < 0) {
        owner.reset();
        media->audio = nullptr;
        return result;
    }
    owner.release();
    return 1;
}

int audio_reset(Audio* audio, double target) {
    if(!audio)
        return 0;
    avcodec_flush_buffers(audio->decoder);
    swr_free(&audio->resampler);
    av_frame_unref(audio->frame);
    av_frame_unref(audio->pcm);
    av_packet_unref(audio->packet);
    audio->flushing_at_end = audio->source_ended = audio->offset = 0;
    audio->seeking = 1;
    audio->written = 0;
    audio->target = audio->next_pts = target;
    output_reset(audio->output, target);
    int result;
    while((result = output_poll(audio->output, 1)) == 0 && audio->output->reset) av_usleep(1000);
    return result < 0 ? result : 0;
}

double audio_preroll(Audio* audio) {
    if(!audio || audio->decoder->sample_rate <= 0)
        return 0;
    return std::fmax(audio->decoder->seek_preroll, audio->decoder->frame_size) / audio->decoder->sample_rate;
}

static int write_pcm_samples(Audio* audio) {
    int64_t gap = audio->frame_start + audio->offset - audio->written;
    if(gap < 0) {
        int skip = static_cast<int>(std::fmin(-gap, audio->pcm->nb_samples - audio->offset));
        audio->offset += skip;
    }
    int remaining = audio->pcm->nb_samples - audio->offset;
    if(!remaining)
        return 0;
    static constexpr std::array<float, 2048> silence{};
    const float* samples = reinterpret_cast<const float*>(audio->pcm->data[0]) + audio->offset * AUDIO_CHANNELS;
    int count = remaining;
    if(gap > 0) {
        samples = silence.data();
        count = static_cast<int>(std::fmin(gap, 1024));
    }
    const float* scaled = samples;
    if(count && output_apply_gain(audio->output, &scaled, count) < 0)
        return AVERROR(ENOMEM);
    int result = output_write(audio->output, scaled, count);
    if(result > 0) {
        audio->written += result;
        if(gap <= 0)
            audio->offset += result;
    }
    return result;
}

int audio_tick(Media* media, int paused, double* clock, double* limit, int* done) {
    Audio* audio = media->audio;
    *clock = -1;
    *limit = 0;
    *done = !audio;
    if(!audio)
        return 0;
    int result = output_poll(audio->output, paused);
    if(result < 0)
        return result;
    int progress = 0;
    if(result > 0) {
        if(audio->offset < audio->pcm->nb_samples) {
            result = write_pcm_samples(audio);
            if(result < 0)
                return result;
            progress = result > 0;
        } else if(!audio->source_ended) {
            result = audio_decode(media);
            if(result < 0)
                return result;
            progress = result;
        } else {
            result = output_finish(audio->output);
            if(result < 0)
                return result;
            *done = result;
        }
    }
    result = output_time(audio->output, clock, limit);
    return result < 0 ? result : progress;
}

static media::Result tick_audio(Media& state, int paused, double* clock, double* limit, int* done) {
    Media* media = &state;
    if(media->audio_unavailable) {
        if(done)
            *done = 1;
        return {0, {}};
    }
    int64_t started = media_trace_start(media);
    int result = audio_tick(media, paused, clock, limit, done);
    media_trace_end(started, &media->trace.audio_us);
    if(result >= 0)
        return {result, {}};
    if((result == AVERROR_EXTERNAL || result == AVERROR(ETIMEDOUT)) && media->audio && media->audio->output) {
        // PulseAudio stopped answering, the device went away, or a stream operation did not
        // finish in time. Losing the sound is a nuisance; stopping the film is not acceptable,
        // so report it once, mark the audio finished and let the video play on in silence.
        // The context is normally healthy here and its error says OK, which is true and useless: a
        // stream's error lives on its context, and when the failure came from our own side there is
        // no PulseAudio error recorded at all. Report the result itself, the PulseAudio error when
        // there is one, and the states, so the next log names the fault.
        auto* output = media->audio->output;
        const int pulse_error = output->stream ? pa_context_errno(pa_stream_get_context(output->stream)) : 0;
        const int stream_state = output->stream ? static_cast<int>(pa_stream_get_state(output->stream)) : -1;
        const int context_state =
                output->context ? static_cast<int>(pa_context_get_state(output->context)) : -1;
        const char* server = output->context ? pa_context_get_server(output->context) : nullptr;
        const char* device = output->stream ? pa_stream_get_device_name(output->stream) : nullptr;
        fprintf(stderr,
                "Audio: stream failed (result=%d, pulse=%s); server=%s device=%s stream_state=%d "
                "context_state=%d; continuing without sound\n",
                result, pulse_error ? pa_strerror(pulse_error) : "none recorded", server ? server : "unknown",
                device ? device : "none", stream_state, context_state);
        media->audio_unavailable = 1;
        if(done)
            *done = 1;
        return {0, {}};
    }
    return media::failure("decode/output audio", result);
}

static media::Result set_volume(Media* media, float volume, int muted) {
    if(!media || !media->audio || !media->audio->output)
        return {-1, "no audio stream"};
    if(output_set_volume(media->audio->output, volume) < 0)
        return media::failure("set volume", AVERROR(EINVAL));
    output_set_muted(media->audio->output, muted);
    return {0, {}};
}

media::Result media::Session::enable_audio() {
    int result = audio_open(get());
    return result < 0 ? failure("open audio", result) : Result{result, {}};
}

media::AudioTick media::Session::audio(bool paused) {
    AudioTick tick{};
    tick.result = tick_audio(*get(), paused, &tick.clock, &tick.limit, &tick.done);
    return tick;
}

bool media::Session::has_audio() const {
    return get() && get()->audio_present;
}

media::Result media::Session::volume(float volume, bool muted) {
    return set_volume(get(), volume, muted);
}
