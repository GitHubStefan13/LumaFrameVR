#include "audio_output.h"
#include "media_trace.h"
#include "audio_owned_operation.hpp"
#include <cmath>
#include <memory>
#include <new>

AudioOutput* output_open(void) {
    std::unique_ptr<AudioOutput> owner(new(std::nothrow) AudioOutput());
    auto* output = owner.get();
    if(!output)
        return nullptr;
    output->loop = pa_mainloop_new();
    if(!output->loop)
        return nullptr;
    output->context = pa_context_new(pa_mainloop_get_api(output->loop), "LumaFrameVR");
    if(!output->context || pa_context_connect(output->context, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr) < 0)
        return nullptr;
    output->corked = 1;
    output->volume = 1.0f;
    output->deadline = av_gettime_relative() + 5000000;
    return owner.release();
}

void output_close(AudioOutput* output) {
    std::unique_ptr<AudioOutput> owner(output);
}

AudioOutput::~AudioOutput() {
    std::vector<float>().swap(gain_);
    media::clear_operation(operation, true);
    media::clear_operation(drain, true);
    if(stream) {
        pa_stream_disconnect(stream);
        pa_stream_unref(stream);
    }
    if(context) {
        pa_context_disconnect(context);
        pa_context_unref(context);
    }
    if(loop)
        pa_mainloop_free(loop);
}

static void note_underflow(pa_stream* stream, void* userdata) {
    (void)stream;
    auto* output = static_cast<AudioOutput*>(userdata);
    if(output->trace)
        output->trace->audio_underflows++;
}

static int connect_playback_stream(AudioOutput* output) {
    pa_sample_spec spec = {PA_SAMPLE_FLOAT32NE, AUDIO_RATE, AUDIO_CHANNELS};
    output->stream = pa_stream_new(output->context, "Video audio", &spec, nullptr);
    if(!output->stream)
        return AVERROR_EXTERNAL;
    pa_stream_set_underflow_callback(output->stream, note_underflow, output);
    uint32_t buffer = AUDIO_RATE * AUDIO_FRAME_BYTES / 4;
    pa_buffer_attr attr = {buffer, buffer, static_cast<uint32_t>(-1), static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};
    auto flags = static_cast<pa_stream_flags_t>(PA_STREAM_START_CORKED | PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING);
    if(pa_stream_connect_playback(output->stream, nullptr, &attr, flags, nullptr, nullptr) < 0)
        return AVERROR_EXTERNAL;
    return 0;
}

int output_poll(AudioOutput* output, int paused) {
    return output->poll(paused);
}

int AudioOutput::poll(bool paused) {
    auto* output = this;
    if(pa_mainloop_iterate(output->loop, 0, nullptr) < 0)
        return AVERROR_EXTERNAL;
    pa_context_state_t context = pa_context_get_state(output->context);
    if(!PA_CONTEXT_IS_GOOD(context))
        return AVERROR_EXTERNAL;
    if(context == PA_CONTEXT_READY && !output->stream) {
        int result = connect_playback_stream(output);
        if(result < 0)
            return result;
        // The deadline belongs to the wait that is running now, not to the moment
        // the output was opened. Arming it once at open made every later connect,
        // flush or cork fail instantly after five seconds of uptime.
        output->deadline = av_gettime_relative() + 5000000;
    }
    if(!output->stream || pa_stream_get_state(output->stream) == PA_STREAM_CREATING) {
        return av_gettime_relative() > output->deadline ? AVERROR(ETIMEDOUT) : 0;
    }
    if(pa_stream_get_state(output->stream) != PA_STREAM_READY)
        return AVERROR_EXTERNAL;
    return output_control(output, paused);
}

int output_write(AudioOutput* output, const float* samples, int frames) {
    size_t writable = pa_stream_writable_size(output->stream);
    if(writable == static_cast<size_t>(-1))
        return AVERROR_EXTERNAL;
    if(writable / AUDIO_FRAME_BYTES < static_cast<size_t>(frames))
        frames = writable / AUDIO_FRAME_BYTES;
    if(!frames)
        return 0;
    if(pa_stream_write(output->stream, samples, frames * AUDIO_FRAME_BYTES, nullptr, 0, PA_SEEK_RELATIVE) < 0)
        return AVERROR_EXTERNAL;
    output->end += frames / static_cast<double>(AUDIO_RATE);
    return frames;
}

int AudioOutput::write(std::span<const float> samples) {
    return output_write(this, samples.data(), static_cast<int>(samples.size() / AUDIO_CHANNELS));
}

AudioOutputTime AudioOutput::time() {
    AudioOutputTime result{0, clock, clock};
    result.status = output_time(this, &result.clock, &result.limit);
    return result;
}

int output_time(AudioOutput* output, double* clock, double* limit) {
    pa_usec_t latency;
    int negative;
    *limit = output->clock;
    if(output->stream && !output->corked && !output->reset && !output->timing && !output->operation && pa_stream_get_state(output->stream) == PA_STREAM_READY) {
        if(pa_stream_get_latency(output->stream, &latency, &negative) == 0) {
            if(output->trace) {
                output->trace->audio_latency_samples++;
                output->trace->audio_latency_us += negative ? -static_cast<int64_t>(latency) : static_cast<int64_t>(latency);
            }
            double position = output->end - (negative ? 0 : latency / 1000000.0);
            output->clock = std::fmax(output->clock, std::fmin(position, output->end));
            *limit = output->end;
        } else if(pa_context_errno(output->context) != PA_ERR_NODATA) {
            return AVERROR_EXTERNAL;
        }
    }
    *clock = output->clock;
    return 0;
}
