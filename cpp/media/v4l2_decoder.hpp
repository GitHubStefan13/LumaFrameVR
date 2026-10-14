#pragma once
#include "media_types.hpp"
#include "v4l2_format.hpp"
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace media {
// Narrow syscall boundary: production uses Linux, unit tests emulate only the
// driver's observable queue contract. An emulator is not hardware verification.
class V4l2Device {
   public:
    virtual ~V4l2Device() = default;
    virtual int control(unsigned long request, void* argument) = 0;  // 0 or -errno
    virtual int poll_events() = 0;                                   // nonblocking: 1 for POLLPRI, 0 if empty, or -errno
    virtual void* map(uint32_t bytes, uint32_t offset) = 0;
    virtual void unmap(void* address, uint32_t bytes) noexcept = 0;
    virtual uint64_t exported_size(int fd) = 0;
    virtual void close_export(int fd) noexcept = 0;
};
struct V4l2Frame {
    DmaBuf buffer{};
    int64_t timestamp_us = 0;
    // The capture buffer must not be requeued until the last consumer releases it.
    std::shared_ptr<void> owner;
};
// A frame leases its capture buffer until the consumer retires it, so a queue
// frees its buffers (STREAMOFF, exported fds, zero-count REQBUFS) only when the
// last lease dies. A decoder that is about to allocate must therefore let the
// previous generation retire first: REQBUFS on a device whose buffers are still
// held answers ENOMEM, which is what demotes a seek or a later open to software.
// The wait is bounded, and the timeout is reported rather than fatal, so a stuck
// consumer degrades to the existing fallback instead of stalling playback.
// require_released_queues is for a replacement decoder, which needs the buffers
// themselves freed; a seek releases nothing at all, so it waits for nothing.
// LUMAFRAMEVR_LEASE_WAIT_MS overrides the budget (default 250 ms).
// A refused allocation names its queue, the request, the leases outstanding and
// whether the smallest possible request is granted, so "Cannot allocate memory"
// from a two-buffer coded queue can be told from the same errno on a capture pool.
// It also names the processes holding the VPU's nodes, because that memory is
// device wide and another client's hold looks identical to our own leak.
// LUMAFRAMEVR_V4L2_CAPTURE_HEADROOM overrides the capture headroom (default 8).
bool await_capture_leases(bool require_released_queues = true) noexcept;
uint64_t outstanding_capture_leases() noexcept;  // frames still holding a buffer
uint64_t outstanding_capture_queues() noexcept;  // queues whose buffers are not released yet
uint64_t open_decoders() noexcept;               // decoder instances this process still has open
// The /dev/video* nodes, sorted, as the adapter opens them.
std::vector<std::string> v4l2_device_nodes();
// Which processes hold any of these device nodes. The driver's memory is device
// wide, so a refused allocation can belong to another client (Steam's own web
// helper opens one of these nodes) rather than to this player.
std::string vpu_holder_report(const std::vector<std::string>& nodes);
class V4l2Decoder {
   public:
    static std::unique_ptr<V4l2Decoder> open(const std::string& path, V4l2Stream stream);
    V4l2Decoder(std::shared_ptr<V4l2Device> device, V4l2Stream stream);
    ~V4l2Decoder();
    V4l2Decoder(const V4l2Decoder&) = delete;
    V4l2Decoder& operator=(const V4l2Decoder&) = delete;
    // One complete access unit, Annex-B for H264/HEVC (FFmpeg owns conversion).
    // EAGAIN leaves the caller's packet intact. Exceptions carry errno failures.
    int submit(std::span<const uint8_t> packet, int64_t timestamp_us);
    int receive(V4l2Frame& frame);  // 0, -EAGAIN or AVERROR_EOF
    int drain();                    // V4L2_DEC_CMD_STOP, once input is submitted
    // Flush both queues for a seek: STREAMOFF on both, the unleased capture
    // slots requeued, then STREAMON. The driver keeps its coded and capture
    // allocations, so a seek has nothing to allocate and cannot be refused one,
    // and a frame leased across it is requeued by reclaim() when the consumer
    // retires it. EAGAIN only while a reset is already in progress.
    int reset();

   private:
    struct State;
    std::unique_ptr<State> state_;
};
}  // namespace media
