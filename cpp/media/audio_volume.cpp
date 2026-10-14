#include "audio_output.h"
#include <new>
#include <stdexcept>

int AudioOutput::set_volume(float requested) {
    if(requested < 0.0f)
        requested = 0.0f;
    if(requested > 1.0f)
        requested = 1.0f;
    volume = requested;
    return 0;
}

int AudioOutput::set_muted(bool requested) {
    muted = requested ? 1 : 0;
    return 0;
}

int output_set_volume(AudioOutput* output, float volume) {
    return output ? output->set_volume(volume) : AVERROR(EINVAL);
}

int output_set_muted(AudioOutput* output, int muted) {
    return output ? output->set_muted(muted) : AVERROR(EINVAL);
}

int AudioOutput::apply_gain_samples(const float*& samples, int frames) {
    float factor = muted ? 0.0f : volume;
    if(factor >= 1.0f || frames <= 0)
        return 0;
    size_t wanted = static_cast<size_t>(frames) * AUDIO_CHANNELS;
    if(wanted > gain_.size()) {
        try {
            gain_.resize(wanted);
        } catch(const std::bad_alloc&) {
            return AVERROR(ENOMEM);
        } catch(const std::length_error&) {
            return AVERROR(ENOMEM);
        }
    }
    const std::span<const float> source(samples, wanted);
    std::span<float> scaled(gain_.data(), wanted);
    for(size_t i = 0; i < wanted; ++i) scaled[i] = source[i] * factor;
    samples = scaled.data();
    return 1;
}

AudioGain AudioOutput::apply_gain(std::span<const float> samples) {
    samples = samples.first(samples.size() / AUDIO_CHANNELS * AUDIO_CHANNELS);
    const float* scaled = samples.data();
    int result = apply_gain_samples(scaled, static_cast<int>(samples.size() / AUDIO_CHANNELS));
    return {result, {scaled, samples.size()}};
}

int output_apply_gain(AudioOutput* output, const float** samples, int frames) {
    if((output->muted ? 0.0f : output->volume) >= 1.0f || frames <= 0)
        return 0;
    return output->apply_gain_samples(*samples, frames);
}
