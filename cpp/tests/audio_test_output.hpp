#pragma once
#include "audio_output.h"
#include <memory>
#include <string>
#include <vector>

namespace lumaframevr::tests {
// Test-only PulseAudio boundary: runs the real Audio/AudioOutput state machines
// without connecting to a server or device. Wrappers delegate when inactive.
struct AudioTestOperation {
    pa_operation_state_t state = PA_OPERATION_RUNNING;
    pa_stream_success_cb_t callback{};
    void* userdata{};
    bool drain{};
};
class AudioTestOutput {
   public:
    AudioTestOutput();
    ~AudioTestOutput();
    AudioTestOutput(const AudioTestOutput&) = delete;
    AudioTestOutput& operator=(const AudioTestOutput&) = delete;
    pa_operation* request(const char* kind, pa_stream_success_cb_t callback, void* userdata);
    void complete();
    void advance(double seconds);
    void assert_paused() const;
    AudioOutput* output{};
    bool auto_complete = true;
    bool connected = true;
    int confirmed_cork = 1;
    int requested_cork = 1;
    int callback_success = 1;
    double queued_seconds{};
    std::vector<std::string> requests;
    std::vector<float> samples;
    std::vector<std::unique_ptr<AudioTestOperation>> operations;
};
}  // namespace lumaframevr::tests
