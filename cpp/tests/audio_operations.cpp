#include "test_registry.hpp"
#include "test_assertions.hpp"
#include "audio_test_output.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace {
lumaframevr::tests::AudioTestOutput* active{};
pa_stream* fake_stream() {
    return reinterpret_cast<pa_stream*>(active);
}
lumaframevr::tests::AudioTestOperation* operation(pa_operation* value) {
    if(!active)
        return nullptr;
    for(const auto& item : active->operations)
        if(reinterpret_cast<pa_operation*>(item.get()) == value)
            return item.get();
    return nullptr;
}
}  // namespace

namespace lumaframevr::tests {
AudioTestOutput::AudioTestOutput() {
    require(!active, "audio boundary fixture is not nested");
    active = this;
}
AudioTestOutput::~AudioTestOutput() {
    active = nullptr;
}
pa_operation* AudioTestOutput::request(const char* kind, pa_stream_success_cb_t callback, void* userdata) {
    requests.emplace_back(kind);
    auto item = std::make_unique<AudioTestOperation>();
    item->callback = callback;
    item->userdata = userdata;
    item->drain = std::string_view(kind) == "drain";
    auto* result = reinterpret_cast<pa_operation*>(item.get());
    operations.push_back(std::move(item));
    return result;
}
void AudioTestOutput::complete() {
    for(auto& item : operations) {
        if(item->state != PA_OPERATION_RUNNING || (item->drain && queued_seconds > 0))
            continue;
        item->state = PA_OPERATION_DONE;
        if(item->callback)
            item->callback(fake_stream(), callback_success, item->userdata);
    }
    confirmed_cork = callback_success ? requested_cork : -1;
}
void AudioTestOutput::advance(double seconds) {
    if(!confirmed_cork)
        queued_seconds = std::max(0.0, queued_seconds - seconds);
}
void AudioTestOutput::assert_paused() const {
    require(confirmed_cork == 1, "pause is confirmed, not merely requested");
}
}  // namespace lumaframevr::tests

extern "C" {
AudioOutput* __real_output_open();
AudioOutput* __wrap_output_open() {
    if(!active)
        return __real_output_open();
    auto* output = new AudioOutput{};
    output->stream = fake_stream();
    output->corked = 1;
    output->volume = 1;
    active->output = output;
    return output;
}
int __real_pa_mainloop_iterate(pa_mainloop*, int, int*);
int __wrap_pa_mainloop_iterate(pa_mainloop* loop, int block, int* result) {
    if(!active)
        return __real_pa_mainloop_iterate(loop, block, result);
    if(active->auto_complete)
        active->complete();
    return 0;
}
pa_context_state_t __real_pa_context_get_state(const pa_context*);
pa_context_state_t __wrap_pa_context_get_state(const pa_context* context) {
    return active ? (active->connected ? PA_CONTEXT_READY : PA_CONTEXT_FAILED) : __real_pa_context_get_state(context);
}
pa_stream_state_t __real_pa_stream_get_state(const pa_stream*);
pa_stream_state_t __wrap_pa_stream_get_state(const pa_stream* stream) {
    return active && stream == fake_stream() ? (active->connected ? PA_STREAM_READY : PA_STREAM_FAILED) : __real_pa_stream_get_state(stream);
}
pa_operation_state_t __real_pa_operation_get_state(const pa_operation*);
pa_operation_state_t __wrap_pa_operation_get_state(const pa_operation* value) {
    auto* item = operation(const_cast<pa_operation*>(value));
    return item ? item->state : __real_pa_operation_get_state(value);
}
void __real_pa_operation_cancel(pa_operation*);
void __wrap_pa_operation_cancel(pa_operation* value) {
    if(auto* item = operation(value))
        item->state = PA_OPERATION_CANCELLED;
    else
        __real_pa_operation_cancel(value);
}
void __real_pa_operation_unref(pa_operation*);
void __wrap_pa_operation_unref(pa_operation* value) {
    if(!operation(value))
        __real_pa_operation_unref(value);
}
pa_operation* __real_pa_stream_cork(pa_stream*, int, pa_stream_success_cb_t, void*);
pa_operation* __wrap_pa_stream_cork(pa_stream* stream, int cork, pa_stream_success_cb_t callback, void* userdata) {
    if(!active || stream != fake_stream())
        return __real_pa_stream_cork(stream, cork, callback, userdata);
    active->requested_cork = cork;
    return active->request(cork ? "cork" : "uncork", callback, userdata);
}
pa_operation* __real_pa_stream_flush(pa_stream*, pa_stream_success_cb_t, void*);
pa_operation* __wrap_pa_stream_flush(pa_stream* stream, pa_stream_success_cb_t callback, void* userdata) {
    if(!active || stream != fake_stream())
        return __real_pa_stream_flush(stream, callback, userdata);
    active->queued_seconds = 0;
    return active->request("flush", callback, userdata);
}
pa_operation* __real_pa_stream_update_timing_info(pa_stream*, pa_stream_success_cb_t, void*);
pa_operation* __wrap_pa_stream_update_timing_info(pa_stream* stream, pa_stream_success_cb_t callback, void* userdata) {
    if(!active || stream != fake_stream())
        return __real_pa_stream_update_timing_info(stream, callback, userdata);
    return active->request("timing", callback, userdata);
}
pa_operation* __real_pa_stream_drain(pa_stream*, pa_stream_success_cb_t, void*);
pa_operation* __wrap_pa_stream_drain(pa_stream* stream, pa_stream_success_cb_t callback, void* userdata) {
    if(!active || stream != fake_stream())
        return __real_pa_stream_drain(stream, callback, userdata);
    return active->request("drain", callback, userdata);
}
size_t __real_pa_stream_writable_size(const pa_stream*);
size_t __wrap_pa_stream_writable_size(const pa_stream* stream) {
    if(!active || stream != fake_stream())
        return __real_pa_stream_writable_size(stream);
    return static_cast<size_t>(std::max(0.0, 0.25 - active->queued_seconds) * AUDIO_RATE) * AUDIO_FRAME_BYTES;
}
int __real_pa_stream_write(pa_stream*, const void*, size_t, pa_free_cb_t, int64_t, pa_seek_mode_t);
int __wrap_pa_stream_write(pa_stream* stream, const void* data, size_t bytes, pa_free_cb_t release, int64_t offset, pa_seek_mode_t seek) {
    if(!active || stream != fake_stream())
        return __real_pa_stream_write(stream, data, bytes, release, offset, seek);
    require(active->output && !active->output->reset && !active->output->timing && !active->output->operation, "PCM writes wait for reset, timing and cork operations to settle");
    const auto* samples = static_cast<const float*>(data);
    active->samples.insert(active->samples.end(), samples, samples + bytes / sizeof(float));
    active->queued_seconds += bytes / static_cast<double>(AUDIO_RATE * AUDIO_FRAME_BYTES);
    return 0;
}
int __real_pa_stream_get_latency(pa_stream*, pa_usec_t*, int*);
int __wrap_pa_stream_get_latency(pa_stream* stream, pa_usec_t* latency, int* negative) {
    if(!active || stream != fake_stream())
        return __real_pa_stream_get_latency(stream, latency, negative);
    *latency = static_cast<pa_usec_t>(std::llround(active->queued_seconds * 1000000));
    *negative = 0;
    return 0;
}
int __real_pa_stream_disconnect(pa_stream*);
int __wrap_pa_stream_disconnect(pa_stream* stream) {
    return active && stream == fake_stream() ? 0 : __real_pa_stream_disconnect(stream);
}
void __real_pa_stream_unref(pa_stream*);
void __wrap_pa_stream_unref(pa_stream* stream) {
    if(!active || stream != fake_stream())
        __real_pa_stream_unref(stream);
}
pa_context* __real_pa_stream_get_context(const pa_stream*);
pa_context* __wrap_pa_stream_get_context(const pa_stream* stream) {
    return active && stream == fake_stream() ? nullptr : __real_pa_stream_get_context(stream);
}
int __real_pa_context_errno(const pa_context*);
int __wrap_pa_context_errno(const pa_context* context) {
    return active && !context ? 0 : __real_pa_context_errno(context);
}
const char* __real_pa_stream_get_device_name(const pa_stream*);
const char* __wrap_pa_stream_get_device_name(const pa_stream* stream) {
    return active && stream == fake_stream() ? "test-only-output" : __real_pa_stream_get_device_name(stream);
}
}  // extern "C"

int lumaframevr::tests::audio_operations(int, char**) {
    return acceptance_main([] {
        AudioTestOutput boundary;
        std::unique_ptr<AudioOutput> output(output_open());
        boundary.auto_complete = false;
        require(output->control(false) == 0 && output->operation, "uncork starts asynchronous operation");
        require(output->control(false) == 0 && boundary.requests.size() == 1, "pending operation blocks new control requests");
        output->deadline = av_gettime_relative() - 1;
        require(output->control(false) == AVERROR(ETIMEDOUT), "pending operation timeout is preserved");
        boundary.complete();
        require(output->control(false) == 1 && !output->operation, "completed uncork releases operation");
        const std::array<float, 4> samples{0.25f, -0.25f, 0.5f, -0.5f};
        require(output->write(samples) == 2 && boundary.samples.size() == 4, "settled output accepts exact PCM");
        output->reset_to(0.25);
        require(output->control(true) == 0 && boundary.requests.back() == "cork", "reset corks before flush");
        boundary.complete();
        boundary.assert_paused();
        require(output->control(true) == 0 && boundary.requests.back() == "flush", "confirmed cork precedes flush");
        boundary.complete();
        require(output->control(true) == 0 && boundary.requests.back() == "timing", "flush precedes timing refresh");
        boundary.complete();
        require(output->control(true) == 1 && !output->reset && !output->timing && !output->operation, "reset settles while paused");
        const auto paused = output->time();
        require(paused.status == 0 && paused.clock == 0.25 && paused.limit == paused.clock, "paused reset fixes both clocks at target");
        require(output->control(false) == 0 && boundary.requests.back() == "uncork", "resume follows all reset operations");
        boundary.complete();
        require(output->control(false) == 1 && output->write(samples) == 2, "resume writes only after confirmation");
        output->end = 1;
        require(output->finish() == 0 && output->drain, "drain is asynchronous");
        auto* drain = operation(output->drain);
        output->reset_to(0.5);
        require(!output->drain && drain->state == PA_OPERATION_CANCELLED && !output->drained, "seek cancels pending drain");
        require(output->control(true) == 0, "next reset starts cork");
        boundary.callback_success = 0;
        boundary.complete();
        require(output->control(true) == AVERROR_EXTERNAL, "failed operation retains failure status");
    });
}
