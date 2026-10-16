#include "v4l2_decoder.hpp"
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <limits.h>
#include <filesystem>
#include <sys/mman.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>
extern "C" {
#include <libavutil/error.h>
}

namespace {
constexpr auto output_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
constexpr auto capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
constexpr uint32_t max_buffers = 64;
// A refused allocation is retried this many times, this far apart, before the
// file falls back to software. Bounded on purpose: the wait is only worth having
// because the refused memory can belong to another client of the VPU.
constexpr uint32_t buffer_attempts = 3;
// Frames a seek may cost before the decoder is written off for the file.
constexpr uint32_t flush_error_budget = 8;
constexpr uint32_t retry_interval_ms = 250;
// Requests include capture headroom for reference/reordered frames and retained
// consumer leases. REQBUFS readback, not these targets, determines queue sizes.
constexpr uint32_t output_buffers = 2;
constexpr uint32_t capture_buffers = 8;
// The geometries whose driver kept its own capture size when asked for the import's margin. Process
// wide, like the VPU write-off and for the same reason: the driver is the same driver for the same
// geometry, and the queue is rebuilt for every file and every seek, so asking again would re-negotiate
// the capture format, an ioctl and at worst a reallocation, for an answer already given. Guarded,
// because a queue can be built on a decoder thread. v4l2_forget_capture_size_declines() empties it for
// tests that need to see the first answer again.
struct CaptureSizeMemo {
    std::mutex mutex;
    std::set<std::array<uint64_t, 4>> declined;
};
CaptureSizeMemo& capture_size_memo() {
    // Immortal on purpose, like the lease registry: a decoder teardown after static destruction must
    // not touch freed memory.
    static CaptureSizeMemo* memo = new CaptureSizeMemo();
    return *memo;
}
bool capture_size_declined(const std::array<uint64_t, 4>& geometry) {
    auto& memo = capture_size_memo();
    const std::lock_guard<std::mutex> guard(memo.mutex);
    return memo.declined.count(geometry) != 0;
}
void remember_capture_size_declined(const std::array<uint64_t, 4>& geometry) {
    auto& memo = capture_size_memo();
    const std::lock_guard<std::mutex> guard(memo.mutex);
    memo.declined.insert(geometry);
}
std::atomic<uint64_t> next_pool{1};
// A frame leases its capture buffer until the consumer retires it, and the queue
// frees its buffers only when the last lease dies. Both counters are process
// wide because a replacement decoder cannot see the previous generation's
// frames: it can only wait for that generation to disappear before it asks the
// driver for buffers of its own. The queue counter is decremented at the end of
// ~CapturePool, i.e. after the buffers are actually released, which is the
// signal the waiter needs; a counter on the pool itself cannot report that,
// because a dying object's reference count reaches zero before its destructor
// has finished.
struct LeaseRegistry {
    std::mutex mutex;
    std::condition_variable retired;
    uint64_t leases = 0;    // frames holding a capture buffer
    uint64_t queues = 0;    // capture queues whose buffers are not released yet
    uint64_t decoders = 0;  // decoder instances open in this process, any queue
};
LeaseRegistry& lease_registry() {
    // Immortal on purpose: a frame owned by a process-lifetime cache can retire
    // after static destruction, and its destructor must not touch freed memory.
    static LeaseRegistry* registry = new LeaseRegistry();
    return *registry;
}
void lease_taken() {
    auto& registry = lease_registry();
    std::lock_guard lock(registry.mutex);
    ++registry.leases;
}
void lease_retired() {
    auto& registry = lease_registry();
    std::lock_guard lock(registry.mutex);
    --registry.leases;
    registry.retired.notify_all();
}
void queue_created() {
    auto& registry = lease_registry();
    std::lock_guard lock(registry.mutex);
    ++registry.queues;
}
void decoder_opened() {
    auto& registry = lease_registry();
    std::lock_guard lock(registry.mutex);
    ++registry.decoders;
}
void decoder_closed() {
    auto& registry = lease_registry();
    std::lock_guard lock(registry.mutex);
    --registry.decoders;
}
void queue_retired() {
    auto& registry = lease_registry();
    std::lock_guard lock(registry.mutex);
    --registry.queues;
    registry.retired.notify_all();
}
bool queues_free(const LeaseRegistry& registry, bool require_released_queues) {
    return registry.leases == 0 && (!require_released_queues || registry.queues == 0);
}
uint64_t lease_wait_ms() {
    const char* value = std::getenv("LUMAFRAMEVR_LEASE_WAIT_MS");
    const long ms = value ? std::strtol(value, nullptr, 10) : 0;
    return ms > 0 ? static_cast<uint64_t>(ms) : 250;
}
// Capture headroom is what is requested on top of the driver's own minimum: room
// for reference/reordered pictures and for the frames the consumer still holds.
// Eight has been right for every stream measured, but a very large extent can be
// refused outright, so the count is overridable to test that on the device
// without another build. Only the request changes; the driver's answer is still
// what sizes the queue.
// A seek stops the queues only when asked to: this driver refuses to restart
// capture, so the default leaves it streaming (see State::flush).
bool seek_restart() {
    const char* value = std::getenv("LUMAFRAMEVR_V4L2_SEEK_RESTART");
    return value && std::strtol(value, nullptr, 10) != 0;
}
uint32_t capture_headroom() {
    const char* value = std::getenv("LUMAFRAMEVR_V4L2_CAPTURE_HEADROOM");
    if(!value)
        return capture_buffers;
    const long count = std::strtol(value, nullptr, 10);
    return count >= 0 && count <= 32 ? static_cast<uint32_t>(count) : capture_buffers;
}
[[noreturn]] void failed(int code, const char* operation) {
    throw std::system_error(code, std::generic_category(), operation);
}
void checked(media::V4l2Device& device, unsigned long request, void* argument, const char* operation) {
    const int result = device.control(request, argument);
    if(result < 0)
        failed(-result, operation);
}
v4l2_buffer buffer(v4l2_buf_type type, uint32_t index, v4l2_plane& plane) {
    v4l2_buffer b{};
    b.type = type;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = index;
    b.length = 1;
    b.m.planes = &plane;
    return b;
}
uint32_t allocate(media::V4l2Device& device, v4l2_buf_type type, uint32_t count, const char* queue, uint64_t bytes, v4l2_memory memory = V4L2_MEMORY_MMAP) {
    // The driver's memory is device wide, so a request that fits on a free device
    // can be refused while another client holds the VPU - Steam's own web helper
    // opens one of these nodes. The retry is short and measured: the log says
    // whether the refusal was transient, which is the difference between waiting
    // and falling back to software for the rest of the file.
    for(uint32_t attempt = 1;; ++attempt) {
        v4l2_requestbuffers req{};
        req.type = type;
        req.memory = memory;
        req.count = count;
        const int result = device.control(VIDIOC_REQBUFS, &req);
        if(result >= 0) {
            if(attempt > 1)
                std::fprintf(stderr, "Decoder: V4L2 %s REQBUFS %u buffer(s) granted on attempt %u after %u ms\n", queue, count, attempt, (attempt - 1) * retry_interval_ms);
            return req.count;
        }
        // A refusal here is either a device with nothing left to give (already
        // streaming elsewhere, or out of memory) or a request it will not grant
        // at this size or count. The message names the queue, the request, what
        // this process still holds, and - for ENOMEM - whether the smallest
        // possible request is granted instead, which separates the two.
        char detail[400];
        const int code = -result;
        if(code == ENOMEM && count && attempt < buffer_attempts) {
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_interval_ms));
            continue;
        }
        if(!count) {
            std::snprintf(detail, sizeof(detail), "V4L2 %s REQBUFS release (zero count) refused", queue);
        } else if(code == ENOMEM) {
            v4l2_requestbuffers probe{};
            probe.type = type;
            probe.memory = memory;
            probe.count = 1;
            const int single = device.control(VIDIOC_REQBUFS, &probe);
            if(single == 0) {
                probe.count = 0;
                device.control(VIDIOC_REQBUFS, &probe);  // give the probe back, it is evidence only
                std::snprintf(detail, sizeof(detail),
                              "V4L2 %s REQBUFS %u buffer(s) of %llu bytes refused %u times over %u ms, "
                              "a single buffer is granted (%llu retained frame lease(s), %llu capture queue(s) outstanding, %llu decoder(s) open here (including this one)",
                              queue, count, static_cast<unsigned long long>(bytes), attempt, (attempt - 1) * retry_interval_ms, static_cast<unsigned long long>(media::outstanding_capture_leases()),
                              static_cast<unsigned long long>(media::outstanding_capture_queues()), static_cast<unsigned long long>(media::open_decoders()));
            } else {
                std::snprintf(detail, sizeof(detail),
                              "V4L2 %s REQBUFS %u buffer(s) of %llu bytes refused %u times over %u ms, and one buffer too (%s) "
                              "(%llu retained frame lease(s), %llu capture queue(s) outstanding, %llu decoder(s) open here (including this one)",
                              queue, count, static_cast<unsigned long long>(bytes), attempt, (attempt - 1) * retry_interval_ms, std::strerror(-single),
                              static_cast<unsigned long long>(media::outstanding_capture_leases()), static_cast<unsigned long long>(media::outstanding_capture_queues()),
                              static_cast<unsigned long long>(media::open_decoders()));
            }
        } else {
            std::snprintf(detail, sizeof(detail),
                          "V4L2 %s REQBUFS %u buffer(s) of %llu bytes refused (%llu retained frame lease(s), "
                          "%llu capture queue(s) outstanding, %llu decoder(s) open here (including this one)",
                          queue, count, static_cast<unsigned long long>(bytes), static_cast<unsigned long long>(media::outstanding_capture_leases()),
                          static_cast<unsigned long long>(media::outstanding_capture_queues()), static_cast<unsigned long long>(media::open_decoders()));
        }
        std::string message = detail;
        if(code == ENOMEM) {
            // The device's memory is wide, and the holder is often on a SIBLING
            // node (Steam's web helper opens /dev/video22, not /dev/video-dec0),
            // so the report covers every video node rather than the one asked.
            const std::string holders = media::vpu_holder_report(media::v4l2_device_nodes());
            if(!holders.empty())
                message += "; the VPU is held by " + holders;
        }
        failed(code, message.c_str());
    }
}
void free_buffers(media::V4l2Device& device, v4l2_buf_type type, v4l2_memory memory = V4L2_MEMORY_MMAP) noexcept {
    v4l2_requestbuffers req{};
    req.type = type;
    req.memory = memory;
    req.count = 0;  // release only, never a count probe
    device.control(VIDIOC_REQBUFS, &req);
}
void stream_off(media::V4l2Device& device, v4l2_buf_type type) noexcept {
    device.control(VIDIOC_STREAMOFF, &type);
}
class LinuxDevice final : public media::V4l2Device {
   public:
    explicit LinuxDevice(const std::string& path) {
        fd_ = ::open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if(fd_ < 0)
            failed(errno, "open V4L2 decoder");
    }
    ~LinuxDevice() override { ::close(fd_); }
    int control(unsigned long request, void* argument) override {
        int result;
        do {
            result = ::ioctl(fd_, request, argument);
        } while(result < 0 && errno == EINTR);
        return result < 0 ? -errno : 0;
    }
    int poll_events() override {
        // V4L2 events signal POLLPRI, not capture-buffer POLLIN. POLLERR can
        // legitimately indicate queues not streaming yet; it is not an event.
        pollfd descriptor{fd_, POLLPRI, 0};
        int result;
        do {
            result = ::poll(&descriptor, 1, 0);
        } while(result < 0 && errno == EINTR);
        if(result < 0)
            return -errno;
        if(descriptor.revents & POLLNVAL)
            return -EBADF;
        if(descriptor.revents & POLLHUP)
            return -ENODEV;
        return (descriptor.revents & POLLPRI) ? 1 : 0;
    }
    void* map(uint32_t bytes, uint32_t offset) override {
        void* address = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, offset);
        if(address == MAP_FAILED)
            failed(errno, "map V4L2 output buffer");
        return address;
    }
    void unmap(void* address, uint32_t bytes) noexcept override { ::munmap(address, bytes); }
    uint64_t exported_size(int fd) override {
        const off_t size = ::lseek(fd, 0, SEEK_END);
        if(size < 0)
            failed(errno, "DMA-BUF allocation size");
        if(::lseek(fd, 0, SEEK_SET) < 0)
            failed(errno, "DMA-BUF reset offset");
        return static_cast<uint64_t>(size);
    }
    void close_export(int fd) noexcept override { ::close(fd); }

   private:
    int fd_ = -1;
};
struct CaptureSlot {
    int fd = -1;
    uint32_t length = 0, data_offset = 0;
    uint64_t size = 0;
    bool queued = false, released = false, leased = false;
};
struct CapturePool {
    std::shared_ptr<media::V4l2Device> device;
    std::vector<CaptureSlot> slots;
    media::V4l2PlaneLayout layout{};
    v4l2_rect crop{};
    std::mutex mutex;
    uint64_t generation = next_pool.fetch_add(1);
    bool allocated = false, streaming = false;
    v4l2_memory allocated_memory = V4L2_MEMORY_MMAP;
    explicit CapturePool(std::shared_ptr<media::V4l2Device> value) : device(std::move(value)) { queue_created(); }
    void stop() noexcept {
        if(streaming)
            stream_off(*device, capture_type);
        streaming = false;
    }
    void stop_checked() {
        if(!streaming)
            return;
        auto type = capture_type;
        checked(*device, VIDIOC_STREAMOFF, &type, "V4L2 capture STREAMOFF");
        streaming = false;
    }
    void release_checked() {
        stop_checked();
        for(auto& slot : slots) {
            if(slot.fd >= 0)
                device->close_export(std::exchange(slot.fd, -1));
        }
        if(allocated) {
            allocate(*device, capture_type, 0, "capture", 0, allocated_memory);
            allocated = false;
        }
    }
    ~CapturePool() {
        stop();
        for(const auto& slot : slots)
            if(slot.fd >= 0)
                device->close_export(slot.fd);
        if(allocated)
            free_buffers(*device, capture_type, allocated_memory);
        // The driver's buffers are released only now. A replacement decoder
        // waiting to allocate is notified from here, after the release.
        queue_retired();
    }
    void queue(uint32_t index) {
        auto& slot = slots.at(index);
        v4l2_plane plane{};
        plane.length = slot.length;
        auto b = buffer(capture_type, index, plane);
        checked(*device, VIDIOC_QBUF, &b, "V4L2 capture QBUF");
        slot.queued = true;
        slot.released = false;
    }
    void reclaim() {
        // Consumer callbacks only mark releases. All ioctls stay on the decode
        // thread, including when graphics releases a frame on another thread.
        std::lock_guard lock(mutex);
        for(uint32_t i = 0; i < slots.size(); ++i)
            if(slots[i].released)
                queue(i);
    }
    void restart() {
        // A seek restarts the queue the driver kept. STREAMOFF returned every
        // buffer to us, including the ones this pool still counts as queued, so
        // the pool is queued again but for the slots a frame still leases: those
        // go back through reclaim() when their lease dies, which is what keeps the
        // memory a consumer is still reading out of the decoder's reach.
        {
            std::lock_guard lock(mutex);
            for(uint32_t i = 0; i < slots.size(); ++i) {
                if(slots[i].leased)
                    slots[i].queued = false;
                else
                    queue(i);
            }
        }
        // Buffers first, then streaming: a capture queue only resumes with
        // buffers in it.
        auto type = capture_type;
        const int result = device->control(VIDIOC_STREAMON, &type);
        if(result < 0) {
            // A refused restart is either the memory the driver needs alongside
            // this pool, or a queue it will not start short of its minimum. Both
            // are answered by saying what the queue looked like when it refused:
            // how much this pool holds, and how many slots are back in it.
            uint32_t back = 0, leased = 0;
            uint64_t bytes = 0;
            for(const auto& slot : slots) {
                if(slot.queued)
                    ++back;
                if(slot.leased)
                    ++leased;
                bytes += slot.size;
            }
            char detail[320];
            std::snprintf(detail, sizeof(detail), "V4L2 capture STREAMON with %u of %zu slot(s) queued (%u still leased, %llu MiB held): %s", back, slots.size(), leased,
                          static_cast<unsigned long long>(bytes >> 20), std::strerror(-result));
            failed(-result, detail);
        }
        streaming = true;
    }
};
struct CaptureLease {
    std::shared_ptr<CapturePool> pool;
    uint32_t index;
    CaptureLease(std::shared_ptr<CapturePool> value, uint32_t slot) : pool(std::move(value)), index(slot) {
        {
            std::lock_guard lock(pool->mutex);
            pool->slots[index].leased = true;
        }
        lease_taken();
    }
    ~CaptureLease() {
        {
            std::lock_guard lock(pool->mutex);
            pool->slots[index].leased = false;
            pool->slots[index].released = true;
        }
        // The pool's own buffers go when its last owner drops it, which is after
        // this destructor body: a waiter sees the queue retire, not the frame.
        lease_retired();
    }
};
struct OutputSlot {
    void* data = nullptr;
    uint32_t length = 0;
    bool queued = false;
};
}  // namespace

struct media::V4l2Decoder::State {
    std::shared_ptr<V4l2Device> device;
    V4l2Stream stream;
    V4l2Formats formats{};
    std::vector<OutputSlot> output;
    std::shared_ptr<CapturePool> capture;
    // OUTPUT is decode order; CAPTURE is presentation order. Retain AU PTS
    // beyond OUTPUT DQBUF (which only releases coded storage), sorting for B
    // frames and keeping duplicates. Empty LAST buffers consume no timestamp.
    std::multiset<int64_t> pending_pts;
    bool timestamp_reported = false;
    bool output_allocated = false, output_streaming = false;
    bool draining = false, ended = false, changing = false, change_last = false, resetting = false;
    // A seek spends a budget of decode-error frames: a discontinuity can cost the
    // driver its references for the old segment, per buffer, and that is a
    // transient - not a reason to write the VPU off for the rest of the file.
    // The budget is per seek and self-limiting, so a genuinely broken stream still
    // fails, just a few frames later than before.
    bool flushed = false;
    uint32_t flush_errors = 0;
    // A flush stopped and restarted the capture queue, so the sequence the driver
    // would have ended with a LAST buffer was discarded by us. A resolution change
    // reported after that has nothing left to drain and is served at once, or it
    // would wait for a LAST buffer that can never arrive.
    bool capture_flushed = false;
    State(std::shared_ptr<V4l2Device> value, V4l2Stream description) : device(std::move(value)), stream(description) {}
    ~State() {
        if(capture)
            capture->stop();
        if(output_streaming)
            stream_off(*device, output_type);
        for(const auto& slot : output)
            if(slot.data)
                device->unmap(slot.data, slot.length);
        if(output_allocated)
            free_buffers(*device, output_type);
        // Capture exports and their device remain alive until the last frame.
    }
    void initialize() {
        if(!device)
            failed(EINVAL, "null V4L2 device");
        // Ordering, not decode policy: a frame retained from an earlier queue
        // still leases that queue's buffers, and the driver answers ENOMEM to a
        // REQBUFS that would allocate over them. Every live queue here belongs to
        // a previous generation, because this decoder has none yet: the
        // constructor has not made one, and a seek keeps the one it has.
        media::await_capture_leases();
        auto selected = v4l2_formats(stream);
        if(!selected)
            failed(ENOTSUP, "unsupported V4L2 stream");
        formats = *selected;
        v4l2_capability caps{};
        checked(*device, VIDIOC_QUERYCAP, &caps, "V4L2 QUERYCAP");
        const auto capabilities = caps.capabilities & V4L2_CAP_DEVICE_CAPS ? caps.device_caps : caps.capabilities;
        if((capabilities & (V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING)) != (V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING))
            failed(ENOTSUP, "V4L2 device is not a streaming mplane decoder");
        v4l2_event_subscription event{};
        event.type = V4L2_EVENT_SOURCE_CHANGE;
        checked(*device, VIDIOC_SUBSCRIBE_EVENT, &event, "V4L2 subscribe source change");
        v4l2_format f{};
        f.type = output_type;
        auto& p = f.fmt.pix_mp;
        p.width = stream.width;
        p.height = stream.height;
        p.pixelformat = formats.coded;
        p.num_planes = 1;
        p.field = V4L2_FIELD_NONE;
        // A request, not an assumption: map and bounds-check the actual returned
        // sizeimage. Oversized access units are refused without truncation.
        p.plane_fmt[0].sizeimage = 4 * 1024 * 1024;
        checked(*device, VIDIOC_S_FMT, &f, "V4L2 coded S_FMT");
        if(p.pixelformat != formats.coded || p.num_planes != 1 || !p.plane_fmt[0].sizeimage)
            failed(ENOTSUP, "V4L2 changed coded format");
        const uint32_t count = allocate(*device, output_type, output_buffers, "coded", p.plane_fmt[0].sizeimage);
        output_allocated = true;
        if(!count || count > max_buffers)
            failed(ENOMEM, "V4L2 output buffer count");
        output.resize(count);
        for(uint32_t i = 0; i < count; ++i) {
            v4l2_plane plane{};
            auto b = buffer(output_type, i, plane);
            checked(*device, VIDIOC_QUERYBUF, &b, "V4L2 output QUERYBUF");
            if(b.length != 1 || plane.length < p.plane_fmt[0].sizeimage)
                failed(EINVAL, "V4L2 output allocation too small");
            output[i].length = plane.length;
            void* mapped = device->map(plane.length, plane.m.mem_offset);
            if(!mapped || mapped == MAP_FAILED)
                failed(ENOMEM, "map V4L2 output buffer");
            output[i].data = mapped;
        }
    }
    void configure_capture() {
        // Stateful decoders report the allocated extent only after parsing the
        // sequence headers. Allocating from container dimensions beforehand is
        // precisely the capture negotiation failure this module avoids.
        auto pool = std::make_shared<CapturePool>(device);
        v4l2_format f{};
        f.type = capture_type;
        checked(*device, VIDIOC_G_FMT, &f, "V4L2 parsed capture G_FMT");
        // The parsed format may describe another FourCC. Do not carry its
        // plane count, pitch or size into a request for the preferred format.
        const auto parsed = f;
        std::vector<uint32_t> candidates{formats.capture};
        // Q10C is requested by component depth, not discovered: Iris does not
        // enumerate it. Retain the existing NV12 negotiation/fallback sequence.
        for(uint32_t index = 0; formats.capture != v4l2_q10c && index < 256; ++index) {
            v4l2_fmtdesc offered{};
            offered.type = capture_type;
            offered.index = index;
            const int result = device->control(VIDIOC_ENUM_FMT, &offered);
            if(result == -EINVAL || result == -ENOTTY)
                break;
            if(result < 0)
                failed(-result, "V4L2 capture ENUM_FMT");
            if(std::find(candidates.begin(), candidates.end(), offered.pixelformat) == candidates.end())
                candidates.push_back(offered.pixelformat);
            if(index == 255)
                failed(EOVERFLOW, "V4L2 capture format enumeration did not end");
        }
        std::optional<V4l2PlaneLayout> layout;
        int refused = ENOTSUP;
        for(const auto candidate : candidates) {
            f = {};
            f.type = capture_type;
            f.fmt.pix_mp.width = parsed.fmt.pix_mp.width;
            f.fmt.pix_mp.height = parsed.fmt.pix_mp.height;
            f.fmt.pix_mp.field = V4L2_FIELD_NONE;
            f.fmt.pix_mp.pixelformat = candidate;
            int result = device->control(VIDIOC_S_FMT, &f);
            if(result < 0 && result != -EINVAL && result != -ENOTSUP)
                failed(-result, "V4L2 capture S_FMT");
            // Some drivers require explicit sizes. G_FMT, not our arithmetic
            // nor the provisional S_FMT answer, supplies the second proposal.
            f = {};
            f.type = capture_type;
            checked(*device, VIDIOC_G_FMT, &f, "V4L2 negotiated capture G_FMT");
            if(result < 0) {
                result = device->control(VIDIOC_S_FMT, &f);
                if(result < 0 && result != -EINVAL && result != -ENOTSUP)
                    failed(-result, "V4L2 capture driver-layout S_FMT");
                f = {};
                f.type = capture_type;
                checked(*device, VIDIOC_G_FMT, &f, "V4L2 retried capture G_FMT");
                if(result < 0) {
                    refused = -result;
                    continue;
                }
            }
            // G_FMT has no DRM offsets/modifier. Only the documented Q10C
            // mapping may construct a modified P010 export from these values.
            const auto& p = f.fmt.pix_mp;
            layout = v4l2_plane_layout(p.pixelformat, p.width, p.height, p.plane_fmt[0].bytesperline);
            if(p.pixelformat == formats.capture && p.num_planes == 1 && layout && v4l2_buffer_fits(*layout, p.plane_fmt[0].sizeimage) && p.width <= INT32_MAX && p.height <= INT32_MAX)
                break;
            layout.reset();
            refused = ENOTSUP;
        }
        if(!layout) {
            const auto& p = f.fmt.pix_mp;
            std::fprintf(stderr,
                         "Decoder: V4L2 capture readback fourcc=%#x planes=%u extent=%ux%u pitch=%u size=%u; "
                         "no usable contiguous NV12 or documented Q10C layout\n",
                         p.pixelformat, p.num_planes, p.width, p.height, p.plane_fmt[0].bytesperline, p.plane_fmt[0].sizeimage);
            failed(refused, "V4L2 capture negotiation has no exportable layout");
        }
        auto& p = f.fmt.pix_mp;
        // The driver's sizeimage is the budget the hardware writes into for a compressed Q10C capture,
        // and for the same modified format Vulkan asks for a few pages more than that. V4L2 lets a
        // client propose sizeimage on S_FMT, so ask for the bytes the described planes occupy plus a
        // margin and let the readback decide: a grant means the import satisfies Vulkan outright, and
        // a clamp leaves the driver's own number, which the negotiated-layout import can still use.
        // REQBUFS has not run yet, so this is a legal point at which to negotiate. Everything below
        // reads this readback, so a driver that ignores the request changes nothing.
        //
        // A driver that keeps its own size is not asked again for the same geometry: every seek builds
        // a new queue, and re-negotiating the capture format for a request this driver has already
        // declined would buy nothing and cost an ioctl and, at worst, a reallocation per seek. A grant
        // is re-asked every time, because the raised format is what makes that queue's buffers big
        // enough in the first place.
        if(layout->modifier == v4l2_qcom_compressed) {
            const uint64_t requested = v4l2_requested_capture_size(*layout);
            const auto geometry = std::array<uint64_t, 4>{p.pixelformat, p.width, p.height, p.plane_fmt[0].bytesperline};
            if(requested > p.plane_fmt[0].sizeimage && requested <= UINT32_MAX && !capture_size_declined(geometry)) {
                const v4l2_format before = f;
                v4l2_format raised = f;
                raised.fmt.pix_mp.plane_fmt[0].sizeimage = static_cast<uint32_t>(requested);
                v4l2_format granted{};
                granted.type = capture_type;
                const bool answered = device->control(VIDIOC_S_FMT, &raised) >= 0 && device->control(VIDIOC_G_FMT, &granted) >= 0;
                const auto& g = granted.fmt.pix_mp;
                auto raised_layout = answered ? v4l2_plane_layout(g.pixelformat, g.width, g.height, g.plane_fmt[0].bytesperline) : std::optional<V4l2PlaneLayout>{};
                if(answered && g.pixelformat == formats.capture && g.num_planes == 1 && raised_layout && v4l2_buffer_fits(*raised_layout, g.plane_fmt[0].sizeimage)) {
                    f = granted;
                    layout = raised_layout;
                } else {
                    // The raise changed something this code does not describe, so keep the format whose
                    // layout was just validated rather than trading a working queue for a margin.
                    f = before;
                }
                const bool granted_larger = f.fmt.pix_mp.plane_fmt[0].sizeimage >= requested;
                if(!granted_larger)
                    remember_capture_size_declined(geometry);
                std::fprintf(stderr, "Decoder: V4L2 capture size request: asked %llu bytes for the Q10C capture, driver answered %llu%s\n", static_cast<unsigned long long>(requested),
                             static_cast<unsigned long long>(f.fmt.pix_mp.plane_fmt[0].sizeimage),
                             granted_larger ? " (using the larger allocation)" : " (clamped; the negotiated planes are imported as reported, and this driver is not asked again for this geometry)");
            }
        }
        pool->layout = *layout;
        v4l2_selection selection{};
        selection.type = capture_type;
        selection.target = V4L2_SEL_TGT_COMPOSE;
        checked(*device, VIDIOC_G_SELECTION, &selection, "V4L2 capture crop");
        const auto& crop = selection.r;
        if(crop.left < 0 || crop.top < 0 || !crop.width || !crop.height || uint64_t(crop.left) + crop.width > p.width || uint64_t(crop.top) + crop.height > p.height)
            failed(EINVAL, "V4L2 invalid capture crop");
        pool->crop = crop;
        // Request one DMABUF first so the driver reports its own minimum, then
        // switch to an MMAP/export pool using the returned count plus headroom.
        const uint32_t headroom = capture_headroom();
        const uint32_t required = allocate(*device, capture_type, 1, "capture", p.plane_fmt[0].sizeimage, V4L2_MEMORY_DMABUF);
        pool->allocated = true;
        pool->allocated_memory = V4L2_MEMORY_DMABUF;
        if(!required || required > max_buffers - headroom)
            failed(ENOMEM, "V4L2 capture DMABUF buffer count");
        const uint32_t count = allocate(*device, capture_type, required + headroom, "capture", p.plane_fmt[0].sizeimage);
        pool->allocated_memory = V4L2_MEMORY_MMAP;
        if(!count || count > max_buffers)
            failed(ENOMEM, "V4L2 capture buffer count");
        pool->slots.resize(count);
        for(uint32_t i = 0; i < count; ++i) {
            auto& slot = pool->slots[i];
            v4l2_plane plane{};
            auto b = buffer(capture_type, i, plane);
            checked(*device, VIDIOC_QUERYBUF, &b, "V4L2 capture QUERYBUF");
            if(b.length != 1 || plane.length < p.plane_fmt[0].sizeimage || plane.data_offset >= plane.length || !v4l2_buffer_fits(*layout, uint64_t(plane.length) - plane.data_offset))
                failed(EINVAL, "V4L2 capture allocation too small");
            slot.length = plane.length;
            slot.data_offset = plane.data_offset;
            v4l2_exportbuffer exp{};
            exp.type = capture_type;
            exp.index = i;
            exp.plane = 0;
            exp.flags = O_CLOEXEC | O_RDWR;
            checked(*device, VIDIOC_EXPBUF, &exp, "V4L2 capture EXPBUF");
            slot.fd = exp.fd;
            if(slot.fd < 0)
                failed(EINVAL, "V4L2 invalid export fd");
            slot.size = device->exported_size(slot.fd);
            if(slot.size < slot.length || !v4l2_buffer_fits(*layout, slot.size - slot.data_offset))
                failed(EINVAL, "V4L2 exported allocation too small");
            pool->queue(i);
        }
        auto type = capture_type;
        checked(*device, VIDIOC_STREAMON, &type, "V4L2 capture STREAMON");
        pool->streaming = true;
        // A fresh pool carries a fresh sequence, so a change reported against it
        // is drained to LAST like any other.
        capture_flushed = false;
        // What this pool actually costs the VPU, and how much of it was our choice:
        // at large extents the driver's own minimum dominates, and REQBUFS readback
        // is what sizes the queue, so neither number can be inferred from the
        // headroom that asked for it.
        {
            uint64_t bytes = 0;
            for(const auto& slot : pool->slots) bytes += slot.size;
            const uint64_t each = pool->slots.empty() ? 0 : pool->slots.front().size;
            std::fprintf(stderr, "Decoder: V4L2 capture pool: %zu buffer(s) of %llu bytes each, %llu MiB total (driver minimum %u, headroom %u)\n", pool->slots.size(),
                         static_cast<unsigned long long>(each), static_cast<unsigned long long>(bytes >> 20), required, headroom);
        }
        capture = std::move(pool);
    }
    void events() {
        for(;;) {
            const int ready = device->poll_events();
            if(ready < 0)
                failed(-ready, "V4L2 poll source change");
            if(!ready)
                return;
            v4l2_event event{};
            const int result = device->control(VIDIOC_DQEVENT, &event);
            // Readiness can become stale; Iris reports an empty queue as ENOENT.
            if(result == -EAGAIN || result == -ENOENT)
                return;
            if(result < 0)
                failed(-result, "V4L2 DQEVENT");
            if(event.type == V4L2_EVENT_SOURCE_CHANGE && (event.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION)) {
                if(!capture) {
                    configure_capture();
                    return;
                }
                changing = true;
                // A flushed pool has no old sequence left to drain, so this change
                // is served without the LAST buffer that would normally end one.
                if(capture_flushed)
                    change_last = true;
            }
        }
    }
    bool replace_capture() {
        if(!changing || !change_last)
            return false;
        capture->stop_checked();
        if(capture.use_count() != 1)
            return true;  // renderer still owns the old pool
        capture->release_checked();
        capture.reset();
        configure_capture();
        changing = change_last = false;
        return false;
    }
    int64_t presentation_timestamp(const v4l2_buffer& b) {
        const uint32_t type = b.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK;
        const bool copied = type == V4L2_BUF_FLAG_TIMESTAMP_COPY;
        if(!timestamp_reported) {
            const char* name = type == V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN ? "UNKNOWN" : type == V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC ? "MONOTONIC" : copied ? "COPY" : "UNRECOGNIZED";
#ifdef V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC_RAW
            if(type == V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC_RAW)
                name = "MONOTONIC_RAW";
#endif
#ifdef V4L2_BUF_FLAG_TIMESTAMP_BOOTTIME
            if(type == V4L2_BUF_FLAG_TIMESTAMP_BOOTTIME)
                name = "BOOTTIME";
#endif
            std::fprintf(stderr, "Decoder: V4L2 capture timestamp type=%s (mask=%#x); presentation PTS from %s\n", name, type,
                         copied ? "COPY buffer timestamp" : "bitstream access unit (not capture clock)");
            timestamp_reported = true;
        }
        auto pending = pending_pts.begin();
        int64_t pts;
        if(copied) {
            if(b.timestamp.tv_usec < 0 || b.timestamp.tv_usec >= 1000000 || b.timestamp.tv_sec > (INT64_MAX - b.timestamp.tv_usec) / 1000000 || b.timestamp.tv_sec < INT64_MIN / 1000000)
                failed(EINVAL, "V4L2 invalid copied timestamp");
            pts = b.timestamp.tv_sec * 1000000 + b.timestamp.tv_usec;
            const auto matching = pending_pts.find(pts);
            if(matching != pending_pts.end())
                pending = matching;
        } else {
            // No capture-clock arithmetic can recover the stream clock. Use
            // the next presentation-ordered AU, not the most recently submitted
            // packet or its OUTPUT slot (either can already have been reused).
            if(pending == pending_pts.end())
                failed(EINVAL, "V4L2 capture has no pending access-unit PTS");
            pts = *pending;
        }
        if(pending != pending_pts.end())
            pending_pts.erase(pending);
        return pts;
    }
    void reclaim_output() {
        if(!output_streaming)
            return;
        for(;;) {
            v4l2_plane plane{};
            auto b = buffer(output_type, 0, plane);
            const int result = device->control(VIDIOC_DQBUF, &b);
            if(result == -EAGAIN)
                return;
            if(result < 0)
                failed(-result, "V4L2 output DQBUF");
            if(b.index >= output.size() || !output[b.index].queued)
                failed(EINVAL, "V4L2 invalid output index");
            output[b.index].queued = false;
            if(b.flags & V4L2_BUF_FLAG_ERROR)
                failed(EIO, "V4L2 coded buffer decode error");
        }
    }
    void discard_events() {
        // Subscriptions survive STREAMOFF: discard only the obsolete events, not
        // fresh headers from the first AU after the flush.
        for(;;) {
            const int ready = device->poll_events();
            if(ready < 0)
                failed(-ready, "V4L2 reset poll");
            if(!ready)
                break;
            v4l2_event event{};
            const int result = device->control(VIDIOC_DQEVENT, &event);
            if(result == -EAGAIN || result == -ENOENT)
                break;
            if(result < 0)
                failed(-result, "V4L2 reset DQEVENT");
        }
    }
    // A seek on the same stream needs a flush, not a teardown. Both queues stop,
    // the capture slots no frame leases go back, capture streams again, and every
    // coded buffer is still ours to queue: nothing is released, so the driver has
    // no allocation to refuse, and the 8K capture pool is not re-negotiated on
    // every seek. Re-negotiation stays with a resolution change, which events()
    // and replace_capture() serve.
    void flush() {
        // A seek does not stop the queues. This driver will not restart capture
        // even with its own minimum pool, every slot back in the queue and nothing
        // leased - it answers "capture STREAMON: Cannot allocate memory" - so the
        // restart, not the memory, is what it refuses. What a seek does have to
        // drop is our own state: the coded queue's pending timestamps and the drain
        // flags. Frames the driver still emits for the old segment carry pre-seek
        // timestamps and are discarded by the seek target.
        if(seek_restart()) {
            // Kept as the device's comparison: LUMAFRAMEVR_V4L2_SEEK_RESTART=1
            // restores stop-and-restart, which this driver refuses.
            if(capture)
                capture->stop_checked();
            if(output_streaming) {
                auto type = output_type;
                checked(*device, VIDIOC_STREAMOFF, &type, "V4L2 output STREAMOFF");
                output_streaming = false;
            }
            for(auto& slot : output) slot.queued = false;
            if(capture)
                capture->restart();
            // The sequence that would have ended in a LAST buffer was discarded by
            // that stop, so a change already announced cannot wait for one.
            capture_flushed = true;
            if(changing)
                change_last = true;
        }
        discard_events();
        pending_pts.clear();
        draining = ended = false;
        flushed = true;
        flush_errors = 0;
    }
};

void media::v4l2_forget_capture_size_declines() {
    auto& memo = capture_size_memo();
    const std::lock_guard<std::mutex> guard(memo.mutex);
    memo.declined.clear();
}

bool media::await_capture_leases(bool require_released_queues) noexcept {
    auto& registry = lease_registry();
    const uint64_t budget = lease_wait_ms();
    std::unique_lock lock(registry.mutex);
    if(queues_free(registry, require_released_queues))
        return true;
    std::fprintf(stderr, "Decoder: waiting up to %llu ms for %llu retained frame lease(s) and %llu V4L2 capture queue(s) to retire\n", static_cast<unsigned long long>(budget),
                 static_cast<unsigned long long>(registry.leases), static_cast<unsigned long long>(registry.queues));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int64_t>(budget));
    // Two milliseconds is only a guard against a missed notification; every
    // release notifies, so a waiting replacement proceeds as soon as it can.
    while(!queues_free(registry, require_released_queues) && std::chrono::steady_clock::now() < deadline) registry.retired.wait_for(lock, std::chrono::milliseconds(2));
    if(queues_free(registry, require_released_queues))
        return true;
    // Bounded by design: the caller allocates anyway and keeps its existing
    // software fallback rather than stalling playback on a stuck consumer.
    std::fprintf(stderr,
                 "Decoder: %llu retained frame lease(s) and %llu V4L2 capture queue(s) did not retire within %llu ms; "
                 "allocating anyway (REQBUFS may fail with ENOMEM)\n",
                 static_cast<unsigned long long>(registry.leases), static_cast<unsigned long long>(registry.queues), static_cast<unsigned long long>(budget));
    return false;
}
uint64_t media::outstanding_capture_leases() noexcept {
    auto& registry = lease_registry();
    std::lock_guard lock(registry.mutex);
    return registry.leases;
}
uint64_t media::outstanding_capture_queues() noexcept {
    auto& registry = lease_registry();
    std::lock_guard lock(registry.mutex);
    return registry.queues;
}
uint64_t media::open_decoders() noexcept {
    auto& registry = lease_registry();
    std::lock_guard lock(registry.mutex);
    return registry.decoders;
}
std::vector<std::string> media::v4l2_device_nodes() {
    std::vector<std::string> paths;
    std::error_code error;
    for(const auto& entry : std::filesystem::directory_iterator("/dev", error))
        if(entry.path().filename().string().starts_with("video"))
            paths.push_back(entry.path().string());
    std::sort(paths.begin(), paths.end());
    return paths;
}
std::string media::vpu_holder_report(const std::vector<std::string>& nodes) {
    // A refusal can belong to another client: the driver's memory is device wide
    // and Steam's own web helper opens one of these nodes. Naming the holder turns
    // "Cannot allocate memory" into something the user can act on. Own processes
    // only - another user's /proc/<pid>/fd is not readable, and is skipped.
    // The count matters as much as the name: this process always holds the node it
    // is opening, so a second descriptor on it is a decoder that was never closed.
    std::map<std::pair<std::string, std::string>, std::size_t> held;
    std::map<std::string, std::string> names;
    const std::string self = std::to_string(::getpid());
    DIR* proc = opendir("/proc");
    if(!proc)
        return {};
    while(const dirent* entry = readdir(proc)) {
        if(entry->d_name[0] < '0' || entry->d_name[0] > '9')
            continue;
        const std::string pid(entry->d_name);
        const std::string descriptors = "/proc/" + pid + "/fd";
        DIR* fds = opendir(descriptors.c_str());
        if(!fds)
            continue;
        while(const dirent* fd = readdir(fds)) {
            if(fd->d_name[0] < '0' || fd->d_name[0] > '9')
                continue;
            char target[PATH_MAX]{};
            const std::string link = descriptors + "/" + fd->d_name;
            const ssize_t length = readlink(link.c_str(), target, sizeof(target) - 1);
            if(length <= 0)
                continue;
            target[length] = '\0';
            for(const auto& node : nodes) {
                if(node != target)
                    continue;
                ++held[{pid, node}];
                if(names.contains(pid))
                    continue;
                char comm[64]{};
                std::FILE* description = std::fopen(("/proc/" + pid + "/comm").c_str(), "r");
                if(description) {
                    if(std::fgets(comm, sizeof(comm), description))
                        names[pid] = comm;
                    std::fclose(description);
                }
                std::string& name = names[pid];
                while(!name.empty() && (name.back() == '\n' || name.back() == '\r')) name.pop_back();
                if(name.empty())
                    name = "pid " + pid;
                name += " (" + pid + ")" + (pid == self ? " [this process]" : "");
            }
        }
        closedir(fds);
    }
    closedir(proc);
    std::string report;
    for(const auto& [key, count] : held) {
        if(!report.empty())
            report += "; ";
        report += names[key.first] + " holds " + key.second;
        if(count > 1)
            report += " x" + std::to_string(count);
    }
    return report;
}

std::unique_ptr<media::V4l2Decoder> media::V4l2Decoder::open(const std::string& path, V4l2Stream stream) {
    return std::make_unique<V4l2Decoder>(std::make_shared<LinuxDevice>(path), stream);
}
media::V4l2Decoder::V4l2Decoder(std::shared_ptr<V4l2Device> device, V4l2Stream stream) : state_(std::make_unique<State>(std::move(device), stream)) {
    decoder_opened();
    try {
        state_->initialize();
    } catch(...) {
        decoder_closed();  // a decoder that never finished opening is not one of ours
        throw;
    }
}
media::V4l2Decoder::~V4l2Decoder() {
    decoder_closed();
}
int media::V4l2Decoder::submit(std::span<const uint8_t> packet, int64_t timestamp_us) {
    auto& s = *state_;
    if(s.resetting)
        return -EAGAIN;
    if(s.draining || s.ended)
        return AVERROR_EOF;
    if(packet.empty())
        failed(EINVAL, "V4L2 empty access unit");
    s.events();
    if(s.changing)
        return -EAGAIN;
    s.reclaim_output();
    if(s.capture)
        s.capture->reclaim();
    for(uint32_t i = 0; i < s.output.size(); ++i) {
        auto& slot = s.output[i];
        if(slot.queued)
            continue;
        if(packet.size() > slot.length)
            failed(EMSGSIZE, "V4L2 access unit exceeds output allocation");
        std::memcpy(slot.data, packet.data(), packet.size());
        v4l2_plane plane{};
        plane.length = slot.length;
        plane.bytesused = packet.size();
        auto b = buffer(output_type, i, plane);
        b.flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
        b.timestamp.tv_sec = timestamp_us / 1000000;
        b.timestamp.tv_usec = timestamp_us % 1000000;
        if(b.timestamp.tv_usec < 0) {
            --b.timestamp.tv_sec;
            b.timestamp.tv_usec += 1000000;
        }
        checked(*s.device, VIDIOC_QBUF, &b, "V4L2 output QBUF");
        slot.queued = true;
        s.pending_pts.insert(timestamp_us);
        if(!s.output_streaming) {
            auto type = output_type;
            checked(*s.device, VIDIOC_STREAMON, &type, "V4L2 output STREAMON");
            s.output_streaming = true;
        }
        return 0;
    }
    return -EAGAIN;
}
int media::V4l2Decoder::reset() {
    auto& s = *state_;
    if(s.resetting)
        return -EAGAIN;
    s.resetting = true;
    // A seek must not allocate. This device refuses the coded REQBUFS that
    // followed our own seek teardown - ENOMEM three times over 500 ms, while
    // nothing of ours was leased and the only holder of the node was this
    // process - so a seek keeps both allocations and flushes what the queues
    // hold. Nothing here needs a replacement: the coded queue describes the same
    // stream, and a capture pool only changes on a resolution change.
    s.flush();
    s.resetting = false;
    return 0;
}
int media::V4l2Decoder::drain() {
    auto& s = *state_;
    if(s.resetting)
        return -EAGAIN;
    if(s.draining || s.ended)
        return 0;
    s.events();
    if(s.changing)
        return -EAGAIN;
    if(!s.output_streaming) {
        s.ended = true;
        return 0;
    }
    v4l2_decoder_cmd command{};
    command.cmd = V4L2_DEC_CMD_STOP;
    const int result = s.device->control(VIDIOC_DECODER_CMD, &command);
    if(result == -EAGAIN)
        return result;
    if(result < 0)
        failed(-result, "V4L2 decoder STOP");
    s.draining = true;
    return 0;
}
int media::V4l2Decoder::receive(V4l2Frame& frame) {
    auto& s = *state_;
    if(s.resetting)
        return -EAGAIN;
    if(s.ended)
        return AVERROR_EOF;
    s.events();
    s.reclaim_output();
    if(s.replace_capture() || !s.capture)
        return -EAGAIN;
    auto& pool = *s.capture;
    pool.reclaim();
    v4l2_plane plane{};
    auto b = buffer(capture_type, 0, plane);
    const int result = s.device->control(VIDIOC_DQBUF, &b);
    if(result == -EAGAIN)
        return result;
    if(result < 0)
        failed(-result, "V4L2 capture DQBUF");
    if(b.length != 1 || b.index >= pool.slots.size() || !pool.slots[b.index].queued)
        failed(EINVAL, "V4L2 invalid capture index");
    auto& slot = pool.slots[b.index];
    slot.queued = false;
    // Make the lease before validating so error paths also release the buffer.
    auto lease = std::make_shared<CaptureLease>(s.capture, b.index);
    if(b.flags & V4L2_BUF_FLAG_ERROR) {
        // Right after a seek the driver may have no references for the segment it is
        // still draining, and says so per buffer. Drop those frames and keep going:
        // the lease above dies with this return, so the slot is reclaimed. Failing
        // here is what used to turn every seek into a software fallback. Bounded,
        // and only after a flush - an error anywhere else still fails.
        if(s.flushed && s.flush_errors++ < flush_error_budget) {
            std::fprintf(stderr, "Decoder: V4L2 dropped a decode-error frame after the seek (%u of %u); continuing on the VPU\n", s.flush_errors, flush_error_budget);
            return -EAGAIN;
        }
        failed(EIO, "V4L2 capture driver decode error");
    }
    if(b.flags & V4L2_BUF_FLAG_LAST) {
        if(s.changing)
            s.change_last = true;
        else if(s.draining)
            s.ended = true;
        else
            failed(EIO, "V4L2 unexpected LAST buffer");
    }
    if(!plane.bytesused && (b.flags & V4L2_BUF_FLAG_LAST))
        return s.ended ? AVERROR_EOF : -EAGAIN;
    // Standard mplane data_offset is a memory-plane base, not a DRM UV
    // offset (nor the MMAP cookie). Preserve it where QUERYBUF/DQBUF reports
    // one; Q10C's documented metadata mapping still supplies the relative UV.
    const uint64_t data_offset = plane.data_offset ? plane.data_offset : slot.data_offset;
    if(data_offset >= slot.length || plane.bytesused > slot.length || plane.bytesused <= data_offset || !v4l2_buffer_fits(pool.layout, slot.length - data_offset) ||
       (!pool.layout.modifier && plane.bytesused - data_offset < pool.layout.required_bytes))
        failed(EINVAL, "V4L2 invalid capture payload extent");
    V4l2Frame decoded;
    auto& d = decoded.buffer;
    d.fd = slot.fd;
    d.format = pool.layout.format;
    d.size = slot.size;
    d.modifier = pool.layout.modifier;
    d.pool = pool.generation;
    d.source_layout = true;
    d.width = pool.layout.width;
    d.height = pool.layout.height;
    for(size_t i = 0; i < 2; ++i) {
        d.offsets[i] = data_offset + pool.layout.offsets[i];
        d.pitches[i] = pool.layout.pitches[i];
    }
    d.crop[0] = pool.crop.left;
    d.crop[1] = pool.crop.top;
    d.crop[2] = d.width - pool.crop.left - pool.crop.width;
    d.crop[3] = d.height - pool.crop.top - pool.crop.height;
    decoded.timestamp_us = s.presentation_timestamp(b);
    decoded.owner = std::move(lease);
    frame = std::move(decoded);
    return 0;
}
