#pragma once
#include <pulse/pulseaudio.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <libavutil/error.h>
#include <libavutil/time.h>
#ifdef __cplusplus
}
#include <span>
#include <vector>

struct AudioOutputTime {
    int status;
    double clock;
    double limit;
};
struct AudioGain {
    int status;
    // A borrowed view: scaled storage stays valid until gain is applied again.
    std::span<const float> samples;
};
inline constexpr int AUDIO_RATE = 48000;
inline constexpr int AUDIO_CHANNELS = 2;
inline constexpr int AUDIO_FRAME_BYTES = 8;

struct AudioOutput {
    AudioOutput() = default;
    ~AudioOutput();
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    int poll(bool paused);
    int write(std::span<const float> samples);
    AudioOutputTime time();
    void reset_to(double target);
    int control(bool paused);
    int finish();
    int set_volume(float volume);
    int set_muted(bool muted);
    AudioGain apply_gain(std::span<const float> samples);
    int apply_gain_samples(const float*& samples, int frames);

    pa_mainloop* loop;
    pa_context* context;
    pa_stream* stream;
    pa_operation *operation, *drain;
    int success, drained, corked, reset, timing;
    int64_t deadline;
    double end, clock;
    struct MediaTrace* trace;
    float volume;
    int muted;

   private:
    std::vector<float> gain_;
};
#else
enum { AUDIO_RATE = 48000, AUDIO_CHANNELS = 2, AUDIO_FRAME_BYTES = 8 };
typedef struct AudioOutput AudioOutput;
#endif

#ifdef __cplusplus
extern "C" {
#endif
AudioOutput* output_open(void);
void output_close(AudioOutput* output);
void output_reset(AudioOutput* output, double target);
int output_poll(AudioOutput* output, int paused);
int output_write(AudioOutput* output, const float* samples, int frames);
int output_finish(AudioOutput* output);
int output_time(AudioOutput* output, double* clock, double* limit);
int output_control(AudioOutput* output, int paused);
int output_set_volume(AudioOutput* output, float volume);
int output_set_muted(AudioOutput* output, int muted);
int output_apply_gain(AudioOutput* output, const float** samples, int frames);
#ifdef __cplusplus
}
#endif
