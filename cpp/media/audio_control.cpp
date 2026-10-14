#include "audio_output.h"
#include "audio_owned_operation.hpp"

static void completed(pa_stream* stream, int success, void* userdata) {
    (void)stream;
    *static_cast<int*>(userdata) = success ? 1 : -1;
}

void AudioOutput::reset_to(double target) {
    auto* output = this;
    output->reset = 1;
    output->end = output->clock = target;
    output->drained = 0;
    if(output->drain) {
        media::clear_operation(output->drain, true);
    }
}

int AudioOutput::control(bool paused) {
    auto* output = this;
    if(output->drain && paused) {
        media::clear_operation(output->drain, true);
        output->drained = 0;
    }
    if(output->operation) {
        if(pa_operation_get_state(output->operation) == PA_OPERATION_RUNNING)
            return av_gettime_relative() > output->deadline ? AVERROR(ETIMEDOUT) : 0;
        media::clear_operation(output->operation, false);
        if(output->success != 1)
            return AVERROR_EXTERNAL;
    }
    int corked = paused || output->reset || output->timing;
    if(corked != output->corked) {
        output->operation = pa_stream_cork(output->stream, corked, completed, &output->success);
        output->corked = corked;
    } else if(output->reset) {
        output->operation = pa_stream_flush(output->stream, completed, &output->success);
        output->reset = 0;
        output->timing = 1;
    } else if(output->timing) {
        output->operation = pa_stream_update_timing_info(output->stream, completed, &output->success);
        output->timing = 0;
    } else {
        return 1;
    }
    if(!output->operation)
        return AVERROR_EXTERNAL;
    output->success = 0;
    output->deadline = av_gettime_relative() + 5000000;
    return 0;
}

void output_reset(AudioOutput* output, double target) {
    output->reset_to(target);
}

int output_control(AudioOutput* output, int paused) {
    return output->control(paused);
}

int output_finish(AudioOutput* output) {
    return output->finish();
}

int AudioOutput::finish() {
    auto* output = this;
    if(output->end == output->clock)
        output->drained = 1;
    if(output->drained < 0)
        return AVERROR_EXTERNAL;
    if(output->drained) {
        output->clock = output->end;
        return 1;
    }
    if(!output->corked && !output->drain) {
        output->drain = pa_stream_drain(output->stream, completed, &output->drained);
        if(!output->drain)
            return AVERROR_EXTERNAL;
    }
    return 0;
}
