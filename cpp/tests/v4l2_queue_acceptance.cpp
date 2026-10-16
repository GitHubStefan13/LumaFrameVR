#include "test_registry.hpp"
#include "test_assertions.hpp"
#include "v4l2_decoder.hpp"
#include "media_internal.h"
#include "media_session.hpp"
#include "media_owned_frame.hpp"
#include "media_result.hpp"
#include "vulkan_video_image.hpp"
#include "vulkan_dma_layout.hpp"
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <thread>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <system_error>
extern "C" {
#include <libavutil/error.h>
#include <libavutil/hwcontext_drm.h>
}

namespace {
class QueueDevice final : public media::V4l2Device {
   public:
    std::vector<unsigned long> calls;
    std::vector<std::string> lifecycle;
    std::vector<v4l2_requestbuffers> buffer_requests;
    uint32_t output_granted = 2, capture_granted = 8, dmabuf_granted = 1;
    int dmabuf_error = 0, capture_mmap_error = 0;
    bool dmabuf_requested = false;
    uint32_t output_done_index = 0, capture_index = 0;
    bool refuse_zero_count = false;
    std::vector<std::vector<uint8_t>> output{2, std::vector<uint8_t>(4096)};
    int exports = 0, closes = 0, maps = 0, unmaps = 0;
    int capture_queued = 0, output_queued = 0, initial_queues = 0;
    // Capture buffers the driver is holding right now, and how often the capture
    // queue has been started: a flush stops and restarts a queue the driver kept,
    // so only the unleased slots can be queued for that second STREAMON.
    int capture_live = 0, capture_streamons = 0;
    static constexpr unsigned long poll_call = 0;  // distinct from every ioctl
    int empty_event_error = EAGAIN, subscription_error = 0, poll_error = 0, stop_error = 0;
    bool subscribed = false, event_polled = false, stale_event_ready = false;
    bool source_change_event = false, initial_source_event = false;
    bool source = false, output_on = false, capture_on = false, stopped = false;
    bool produce = false, output_done = false, last = false, empty_last = false;
    bool short_buffer = false, short_export = false, wrong_format = false;
    unsigned long fail_request = 0;
    bool error_frame = false, map_failed = false;
    bool capture_set = false, capture_readback = false;
    int first_proposal_error = 0;
    // Emulated ordering evidence: whether the first allocating REQBUFS arrived
    // while a previous queue still held its buffers.
    std::function<bool()> allocation_probe;
    bool allocation_seen = false, allocated_while_leased = false;
    // Emulated refusals: a device that will not grant more than this many buffers
    // (0 = grants everything), and the headroom the MMAP request must carry.
    uint32_t enomem_above = 0, expected_headroom = 8;
    bool refused_above = false;
    uint32_t enomem_times = 0, enomem_refusals = 0;  // a refusal that clears on its own
    int capture_proposals = 0;
    std::vector<unsigned long> capture_format_calls;
    std::vector<v4l2_pix_format_mplane> capture_requests;
    std::vector<uint32_t> offered_capture_formats;
    uint32_t rejected_capture_format = 0, readback_format = 0;
    bool mutated_rejection = false;
    uint32_t coded_format = V4L2_PIX_FMT_H264, capture_format = V4L2_PIX_FMT_NV12;
    uint32_t capture_width = 96, capture_height = 64, capture_pitch = 96, capture_bytes = 9216;
    uint32_t capture_planes = 1;
    std::optional<uint32_t> capture_payload;
    uint32_t query_data_offset = 0, capture_data_offset = 0;
    uint64_t export_bytes = 0;
    timeval timestamp{1, 234567};  // submitted access-unit PTS
    timeval capture_timestamp{1, 234567};
    bool separate_capture_timestamp = false;
    uint32_t timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
    int control(unsigned long request, void* argument) override {
        calls.push_back(request);
        if(request == fail_request)
            return -EIO;
        switch(request) {
            case VIDIOC_QUERYCAP: {
                auto& cap = *static_cast<v4l2_capability*>(argument);
                cap.capabilities = V4L2_CAP_DEVICE_CAPS;
                cap.device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
                return 0;
            }
            case VIDIOC_SUBSCRIBE_EVENT:
                require(static_cast<v4l2_event_subscription*>(argument)->type == V4L2_EVENT_SOURCE_CHANGE, "subscribe source changes");
                if(subscription_error)
                    return -subscription_error;
                subscribed = true;
                return 0;
            case VIDIOC_ENUM_FMT: {
                auto& format = *static_cast<v4l2_fmtdesc*>(argument);
                require(format.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, "enumerate capture formats");
                if(format.index >= offered_capture_formats.size())
                    return -EINVAL;
                format.pixelformat = offered_capture_formats[format.index];
                return 0;
            }
            case VIDIOC_S_FMT:
            case VIDIOC_G_FMT: {
                auto& f = *static_cast<v4l2_format*>(argument);
                lifecycle.push_back(std::string(f.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ? "capture-" : "output-") + (request == VIDIOC_G_FMT ? "get" : "set"));
                auto& p = f.fmt.pix_mp;
                if(f.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
                    require(p.pixelformat == coded_format && p.num_planes == 1, "coded mplane negotiation");
                    p.plane_fmt[0].sizeimage = 4096;
                } else {
                    // The decoder never negotiates capture before the driver has
                    // parsed a sequence, and the first pool only follows headers
                    // the coded queue was streaming. A re-negotiation after a flush
                    // reads the geometry the driver already reported, with the coded
                    // queue stopped, which is what G_FMT is defined to answer.
                    require(source && (capture_streamons > 0 || output_on), "capture format waits for parsed sequence headers");
                    capture_format_calls.push_back(request);
                    if(request == VIDIOC_S_FMT) {
                        require(p.pixelformat == capture_format || std::find(offered_capture_formats.begin(), offered_capture_formats.end(), p.pixelformat) != offered_capture_formats.end(),
                                "capture requests preferred or driver-offered format");
                        capture_requests.push_back(p);
                        ++capture_proposals;
                        if(p.pixelformat == rejected_capture_format) {
                            // The failed ioctl's mutated result is not a readback.
                            p.plane_fmt[0].bytesperline = 7;
                            p.plane_fmt[0].sizeimage = 11;
                            mutated_rejection = true;
                            return -ENOTSUP;
                        }
                        if(first_proposal_error && capture_proposals == 1)
                            return -first_proposal_error;
                        capture_set = true;
                        capture_readback = false;
                    } else if(capture_set)
                        capture_readback = true;
                    p.width = capture_width;
                    p.height = capture_height;
                    p.num_planes = capture_planes;
                    p.pixelformat = readback_format ? readback_format : wrong_format ? v4l2_fourcc('Q', '1', '0', 'C') : capture_format;
                    // S_FMT returns a provisional stride; only readback is authoritative.
                    p.plane_fmt[0].bytesperline = request == VIDIOC_S_FMT ? capture_pitch + 64 : capture_pitch;
                    p.plane_fmt[0].sizeimage = capture_bytes;
                }
                return 0;
            }
            case VIDIOC_REQBUFS: {
                auto& req = *static_cast<v4l2_requestbuffers*>(argument);
                buffer_requests.push_back(req);
                lifecycle.push_back(std::string(req.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ? "capture-" : "output-") + (!req.count                         ? "free"
                                                                                                                            : req.memory == V4L2_MEMORY_DMABUF ? "dmabuf"
                                                                                                                                                               : "mmap"));
                if(!req.count)
                    return refuse_zero_count ? -EINVAL : 0;
                if(enomem_above && req.count > enomem_above) {
                    refused_above = true;
                    return -ENOMEM;
                }
                if(enomem_times && enomem_refusals < enomem_times) {
                    ++enomem_refusals;
                    return -ENOMEM;
                }
                if(!allocation_seen) {
                    allocation_seen = true;
                    allocated_while_leased = allocation_probe && allocation_probe();
                }
                if(req.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
                    require(req.memory == V4L2_MEMORY_MMAP, "coded queue stays MMAP");
                    // One buffer is allowed exactly once, as the probe that follows
                    // a refusal; anything else is the original two-buffer request.
                    if(req.count == 1 && refused_above)
                        refused_above = false;
                    else
                        require(req.count == 2, "request two coded buffers, without a count probe");
                    req.count = output_granted;
                    output.resize(req.count, std::vector<uint8_t>(4096));
                } else {
                    require(source, "capture allocation waits for parsed stream");
                    require(capture_readback, "capture format read back before allocation");
                    if(req.memory == V4L2_MEMORY_DMABUF) {
                        require(req.count == 1, "capture requests one DMABUF before its MMAP pool");
                        if(dmabuf_error)
                            return -dmabuf_error;
                        dmabuf_requested = true;
                        req.count = dmabuf_granted;
                    } else {
                        require(req.memory == V4L2_MEMORY_MMAP && dmabuf_requested && req.count == dmabuf_granted + expected_headroom,
                                "capture MMAP request uses driver DMABUF count plus consumer/reorder headroom");
                        if(capture_mmap_error)
                            return -capture_mmap_error;
                        initial_queues = 0;
                        req.count = capture_granted;
                    }
                }
                return 0;
            }
            case VIDIOC_QUERYBUF: {
                auto& b = *static_cast<v4l2_buffer*>(argument);
                require(b.length == 1 && b.memory == V4L2_MEMORY_MMAP, "query single contiguous memory plane");
                require(b.index < (b.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ? output_granted : capture_granted), "query only a granted buffer index");
                b.m.planes[0].length = b.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ? 4096 : short_buffer ? capture_bytes - 1 : capture_bytes;
                b.m.planes[0].m.mem_offset = b.index * 4096;
                if(b.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
                    b.m.planes[0].data_offset = query_data_offset;
                return 0;
            }
            case VIDIOC_EXPBUF: {
                auto& e = *static_cast<v4l2_exportbuffer*>(argument);
                require(e.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE && e.plane == 0, "export capture memory plane");
                require(e.index < capture_granted, "export only a granted capture buffer");
                e.fd = 100 + e.index;
                ++exports;
                return 0;
            }
            case VIDIOC_G_CTRL:
            case VIDIOC_G_PARM:
                return -EINVAL;  // no minimum-count / streaming-parameter queries supported
            case VIDIOC_G_SELECTION: {
                auto& s = *static_cast<v4l2_selection*>(argument);
                require(s.target == V4L2_SEL_TGT_COMPOSE, "query visible compose rect");
                s.r = {0, 0, 96, 64};
                return 0;
            }
            case VIDIOC_QBUF: {
                auto& b = *static_cast<v4l2_buffer*>(argument);
                require(b.index < (b.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ? output_granted : capture_granted), "queue only a granted buffer index");
                if(b.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
                    require(b.m.planes[0].bytesused == 4, "output access unit size");
                    require(std::equal(output[b.index].begin(), output[b.index].begin() + 4, std::vector<uint8_t>{0, 0, 1, 9}.begin()), "output access unit bytes");
                    require(b.timestamp.tv_sec == timestamp.tv_sec && b.timestamp.tv_usec == timestamp.tv_usec, "timestamp carried to kernel");
                    require((b.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK) == V4L2_BUF_FLAG_TIMESTAMP_COPY, "capture must copy presentation timestamps");
                    ++output_queued;
                } else {
                    ++capture_queued;
                    ++initial_queues;
                    ++capture_live;
                }
                return 0;
            }
            case VIDIOC_STREAMON: {
                auto type = *static_cast<v4l2_buf_type*>(argument);
                if(type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
                    require(subscribed && output_queued > 0, "subscribe and queue input before output streaming");
                    output_on = true;
                    source = true;
                    initial_source_event = true;
                } else {
                    // The first STREAMON follows a fully exported pool; a flush
                    // re-streams the pool the driver kept, so only the unleased
                    // slots can be queued, and a capture queue only resumes with
                    // buffers in it.
                    require(capture_streamons ? capture_live >= 1 : initial_queues == int(capture_granted), "queued exports precede capture streaming");
                    ++capture_streamons;
                    capture_on = true;
                }
                return 0;
            }
            case VIDIOC_STREAMOFF:
                lifecycle.push_back(*static_cast<v4l2_buf_type*>(argument) == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ? "capture-off" : "output-off");
                if(*static_cast<v4l2_buf_type*>(argument) == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                    capture_on = false;
                    capture_live = 0;  // STREAMOFF returns every buffer
                } else
                    output_on = false;
                return 0;
            case VIDIOC_DQEVENT:
                require(event_polled, "DQEVENT requires fresh event readiness, not buffer readiness");
                event_polled = false;
                stale_event_ready = false;
                if(!source_change_event && !initial_source_event)
                    return -empty_event_error;
                source_change_event = initial_source_event = false;
                static_cast<v4l2_event*>(argument)->type = V4L2_EVENT_SOURCE_CHANGE;
                static_cast<v4l2_event*>(argument)->u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION;
                return 0;
            case VIDIOC_DQBUF: {
                auto& b = *static_cast<v4l2_buffer*>(argument);
                if(b.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
                    if(!output_done)
                        return -EAGAIN;
                    output_done = false;
                    b.index = output_done_index;
                    return 0;
                }
                if(!produce)
                    return -EAGAIN;
                produce = false;
                b.index = capture_index;
                if(capture_live)
                    --capture_live;  // the driver's queue cannot go below empty
                b.timestamp = separate_capture_timestamp ? capture_timestamp : timestamp;
                b.flags = timestamp_flags | (last ? V4L2_BUF_FLAG_LAST : 0) | (error_frame ? V4L2_BUF_FLAG_ERROR : 0);
                b.m.planes[0].bytesused = last && empty_last ? 0 : capture_payload.value_or(capture_bytes);
                b.m.planes[0].data_offset = capture_data_offset;
                return 0;
            }
            case VIDIOC_DECODER_CMD:
                require(static_cast<v4l2_decoder_cmd*>(argument)->cmd == V4L2_DEC_CMD_STOP, "EOS uses decoder STOP not zero-sized QBUF");
                if(stop_error)
                    return -stop_error;
                stopped = true;
                return 0;
            default:
                throw std::runtime_error("unexpected V4L2 ioctl");
        }
    }
    int poll_events() override {
        calls.push_back(poll_call);
        require(subscribed, "source changes subscribed before polling");
        if(poll_error)
            return -poll_error;
        event_polled = stale_event_ready || source_change_event || initial_source_event;
        return event_polled ? 1 : 0;
    }
    void* map(uint32_t bytes, uint32_t offset) override {
        require(bytes == 4096 && offset / 4096 < output.size(), "map only output planes");
        if(map_failed)
            return MAP_FAILED;
        lifecycle.push_back("map");
        ++maps;
        return output[offset / 4096].data();
    }
    void unmap(void*, uint32_t) noexcept override {
        lifecycle.push_back("unmap");
        ++unmaps;
    }
    uint64_t exported_size(int) override { return short_export ? capture_bytes - 1 : export_bytes ? export_bytes : capture_bytes; }
    void close_export(int) noexcept override {
        lifecycle.push_back("close");
        ++closes;
    }
};
const media::V4l2Stream stream{media::V4l2Codec::H264, 96, 64, 8};
const std::vector<uint8_t> packet{0, 0, 1, 9};
// The lease budget and the capture headroom are read from the environment at
// every use, exactly as the decode stall budget is, so a case can set either
// without a second API.
struct ScopedEnvironment {
    ScopedEnvironment(const char* variable, const char* value) : variable_(variable), saved_(std::getenv(variable) ? std::getenv(variable) : "") { setenv(variable, value, 1); }
    ~ScopedEnvironment() {
        if(saved_.empty())
            unsetenv(variable_);
        else
            setenv(variable_, saved_.c_str(), 1);
    }
    ScopedEnvironment(const ScopedEnvironment&) = delete;
    ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;
    const char* variable_;
    std::string saved_;
};
// Proves the ordering a replacement decoder must respect: allocate only once
// the previous queue has released its buffers.
std::function<bool()> buffer_probe() {
    return [] { return media::outstanding_capture_queues() != 0; };
}
size_t count_of(const std::string& text, const std::string& needle) {
    size_t found = 0;
    for(size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + needle.size())) ++found;
    return found;
}
}  // namespace

int lumaframevr::tests::v4l2_queue(int, char**) {
    return acceptance_main([] {
        for(uint32_t bits : {8u, 10u})
            for(uint32_t output_count : {2u, 1u, 3u})
                for(uint32_t capture_count : {8u, 3u, 1u, 10u})
                    for(bool refuse_zero : {false, true}) {
                        auto granted = std::make_shared<QueueDevice>();
                        granted->output_granted = output_count;
                        granted->capture_granted = capture_count;
                        granted->dmabuf_granted = capture_count;  // different returned minima
                        granted->output_done_index = output_count - 1;
                        granted->capture_index = capture_count - 1;
                        granted->refuse_zero_count = refuse_zero;
                        if(bits == 10) {
                            granted->capture_format = v4l2_fourcc('Q', '1', '0', 'C');
                            granted->capture_pitch = 256;
                            granted->capture_bytes = 24576;
                        }
                        {
                            media::V4l2Decoder decoder(granted, {media::V4l2Codec::H264, 96, 64, bits});
                            require(granted->maps == int(output_count), "map exactly the OUTPUT count returned by REQBUFS");
                            for(uint32_t i = 0; i < output_count; ++i) require(decoder.submit(packet, 1234567) == 0, "use every granted OUTPUT slot");
                            require(decoder.submit(packet, 1234567) == -EAGAIN, "OUTPUT backpressure uses granted count, not requested count");
                            granted->output_done = true;
                            require(decoder.submit(packet, 1234567) == 0, "last granted OUTPUT slot can be reclaimed and reused");
                            media::V4l2Frame frame, empty;
                            require(decoder.receive(frame) == -EAGAIN && granted->capture_on && granted->exports == int(capture_count) && granted->capture_queued == int(capture_count),
                                    "export and queue exactly the CAPTURE count returned by REQBUFS");
                            require(granted->buffer_requests.size() == 3 && granted->buffer_requests[0].type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE && granted->buffer_requests[0].count == 2 &&
                                            granted->buffer_requests[1].type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE && granted->buffer_requests[1].memory == V4L2_MEMORY_DMABUF &&
                                            granted->buffer_requests[1].count == 1 && granted->buffer_requests[2].type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
                                            granted->buffer_requests[2].memory == V4L2_MEMORY_MMAP && granted->buffer_requests[2].count == capture_count + 8,
                                    "capture DMABUF request precedes MMAP with returned count plus headroom");
                            require(std::count(granted->calls.begin(), granted->calls.end(), VIDIOC_G_CTRL) == 0 && std::count(granted->calls.begin(), granted->calls.end(), VIDIOC_G_PARM) == 0,
                                    "NV12 and Q10C sequences contain no minimum-count queries");
                            granted->produce = true;
                            require(decoder.receive(frame) == 0 && frame.buffer.fd == int(100 + capture_count - 1), "last granted CAPTURE slot delivers a retained frame");
                            require(decoder.receive(empty) == -EAGAIN && granted->capture_queued == int(capture_count), "retaining a frame never prematurely requeues its slot, even with one buffer");
                            frame = {};
                            require(decoder.receive(empty) == -EAGAIN && granted->capture_queued == int(capture_count + 1), "released CAPTURE slot is reclaimed on the decode thread");
                        }
                        require(granted->unmaps == int(output_count) && granted->closes == int(capture_count), "all granted buffers are released even when zero-count release returns EINVAL");
                        require(granted->buffer_requests.size() == 5 && granted->buffer_requests[3].type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE && granted->buffer_requests[3].count == 0 &&
                                        granted->buffer_requests[4].type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE && granted->buffer_requests[4].count == 0 &&
                                        granted->buffer_requests[4].memory == V4L2_MEMORY_MMAP,
                                "zero-count REQBUFS is used only to release allocated queues after decoding");
                    }
        for(int fault = 0; fault < 5; ++fault) {
            auto request = std::make_shared<QueueDevice>();
            if(fault == 0)
                request->dmabuf_error = EIO;
            if(fault == 1)
                request->capture_mmap_error = EIO;
            if(fault == 2)
                request->dmabuf_granted = 0;
            if(fault == 3)
                request->dmabuf_granted = 57;  // cannot add eight buffers within the 64-slot bound
            if(fault == 4)
                request->dmabuf_granted = UINT32_MAX;
            {
                media::V4l2Decoder decoder(request, stream);
                decoder.submit(packet, 1234567);
                media::V4l2Frame frame;
                rejects([&] { decoder.receive(frame); }, "capture DMABUF/MMAP failure or invalid returned count propagates to fallback");
                require(request->exports == 0 && !request->capture_on, "unsafe capture allocation never exports or streams");
                if(fault != 0) {
                    const auto& release = request->buffer_requests.back();
                    require(release.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE && release.memory == V4L2_MEMORY_DMABUF && release.count == 0,
                            "failed MMAP transition or count validation releases the preceding DMABUF allocation");
                }
            }
            require(request->maps == request->unmaps && request->closes == request->exports, "capture mode transition cleanup is balanced");
        }
        for(int fault = 0; fault < 6; ++fault) {
            auto unsafe = std::make_shared<QueueDevice>();
            unsafe->capture_format = v4l2_fourcc('Q', '1', '0', 'C');
            unsafe->capture_pitch = 256;
            unsafe->capture_bytes = 24576;
            if(fault == 0)
                unsafe->capture_planes = 2;
            if(fault == 1)
                unsafe->capture_bytes = 20480;  // UV starts at allocation end
            if(fault == 2)
                unsafe->readback_format = v4l2_fourcc('Q', '0', '8', 'C');
            if(fault == 3)
                unsafe->readback_format = media::v4l2_p010;  // ten-bit request must not silently become linear
            if(fault == 4)
                unsafe->query_data_offset = 8192;
            if(fault == 5)
                unsafe->capture_pitch = 0;
            {
                media::V4l2Decoder decoder(unsafe, {media::V4l2Codec::H264, 96, 64, 10});
                decoder.submit(packet, 1234567);
                media::V4l2Frame frame;
                rejects([&] { decoder.receive(frame); }, "unsupported or undersized compressed capture layout is refused");
                require(unsafe->exports == 0, "invalid Q10C topology, mapping or bounds never reach import");
            }
            require(unsafe->maps == unsafe->unmaps && unsafe->closes == unsafe->exports, "refused Q10C negotiation releases resources");
        }
        {
            auto no_minimum = std::make_shared<QueueDevice>();
            no_minimum->fail_request = VIDIOC_G_CTRL;
            media::V4l2Decoder decoder(no_minimum, stream);
            require(decoder.submit(packet, 1234567) == 0, "prime driver without minimum-count control");
            media::V4l2Frame frame;
            require(decoder.receive(frame) == -EAGAIN && no_minimum->capture_on, "capture allocation does not require a minimum-count query");
            require(std::count(no_minimum->calls.begin(), no_minimum->calls.end(), VIDIOC_G_CTRL) == 0 && std::count(no_minimum->calls.begin(), no_minimum->calls.end(), VIDIOC_G_PARM) == 0,
                    "queue setup contains no minimum-count or streaming-parameter query");
        }
        // The size-request memory is process wide, so this case starts from a clean one and then drives
        // it: the fixture's driver clamps the request, which is exactly the answer that is remembered.
        media::v4l2_forget_capture_size_declines();
        bool size_request_declined = false;
        for(uint32_t bits : {8u, 10u})
            for(int proposal_error : {0, EINVAL, ENOTSUP}) {
                const bool needs_sizes = proposal_error != 0;
                auto negotiation = std::make_shared<QueueDevice>();
                negotiation->first_proposal_error = proposal_error;
                if(bits == 10) {
                    negotiation->capture_format = v4l2_fourcc('Q', '1', '0', 'C');
                    negotiation->capture_pitch = 256;
                    negotiation->capture_bytes = 24576;
                }
                media::V4l2Decoder decoder(negotiation, {media::V4l2Codec::H264, 96, 64, bits});
                require(decoder.submit(packet, 1234567) == 0, "prime capture negotiation");
                media::V4l2Frame frame;
                require(decoder.receive(frame) == -EAGAIN && negotiation->exports == 8, "format-only proposal and driver-size retry both allocate capture");
                const auto& first = negotiation->capture_requests.front();
                require(first.pixelformat == negotiation->capture_format && first.num_planes == 0 && first.plane_fmt[0].bytesperline == 0 && first.plane_fmt[0].sizeimage == 0,
                        "first proposal requests a format, not an inherited or computed layout");
                std::vector<unsigned long> expected = needs_sizes ? std::vector<unsigned long>{VIDIOC_G_FMT, VIDIOC_S_FMT, VIDIOC_G_FMT, VIDIOC_S_FMT, VIDIOC_G_FMT}
                                                                  : std::vector<unsigned long>{VIDIOC_G_FMT, VIDIOC_S_FMT, VIDIOC_G_FMT};
                // A compressed capture whose driver size is below the bytes its planes occupy plus the
                // margin is asked once more for that margin, and the readback decides. This driver clamps
                // it, so the first queue for this geometry asks and the ones after it do not - which is
                // the other half of the contract, and the half that keeps a seek from re-negotiating the
                // capture format for an answer it already has.
                const auto capture_layout = media::v4l2_plane_layout(negotiation->capture_format, negotiation->capture_width, negotiation->capture_height, negotiation->capture_pitch);
                const bool compressed = capture_layout && media::v4l2_requested_capture_size(*capture_layout) > negotiation->capture_bytes;
                const bool asks_for_size = compressed && !size_request_declined;
                if(asks_for_size) {
                    expected.insert(expected.end(), {VIDIOC_S_FMT, VIDIOC_G_FMT});
                    // The request is only meaningful if it reaches the driver with the documented number.
                    require(negotiation->capture_requests.back().plane_fmt[0].sizeimage == static_cast<uint32_t>(media::v4l2_requested_capture_size(*capture_layout)),
                            "the size request proposes the described planes plus the margin for that extent");
                }
                require(negotiation->capture_format_calls == expected, asks_for_size ? "proposal/readback/reproposal ioctl order" : "a geometry this driver has already declined is not asked again");
                if(compressed)
                    size_request_declined = true;
                if(needs_sizes) {
                    const auto& retry = negotiation->capture_requests.at(1);
                    require(retry.pixelformat == negotiation->capture_format && retry.num_planes == 1 && retry.width == negotiation->capture_width && retry.height == negotiation->capture_height &&
                                    retry.plane_fmt[0].bytesperline == negotiation->capture_pitch && retry.plane_fmt[0].sizeimage == negotiation->capture_bytes,
                            "second proposal uses driver G_FMT numbers verbatim");
                }
            }
        {
            auto alternatives = std::make_shared<QueueDevice>();
            alternatives->rejected_capture_format = V4L2_PIX_FMT_NV12;
            const auto offered = v4l2_fourcc('Q', '0', '8', 'C');
            alternatives->offered_capture_formats = {V4L2_PIX_FMT_NV12, offered};
            media::V4l2Decoder decoder(alternatives, stream);
            decoder.submit(packet, 1234567);
            media::V4l2Frame frame;
            require(decoder.receive(frame) == -EAGAIN && alternatives->exports == 8, "refused preference tries another driver-offered format");
            require(alternatives->capture_requests.size() == 3 && alternatives->mutated_rejection, "preference and driver-sized retry precede the usable alternative");
            const auto& retry = alternatives->capture_requests[1];
            require(retry.plane_fmt[0].bytesperline == alternatives->capture_pitch && retry.plane_fmt[0].sizeimage == alternatives->capture_bytes, "retry ignores mutated failed ioctl result");
            require(alternatives->capture_requests[2].pixelformat == offered, "alternative proposes the offered FourCC, not a relabelled layout");
            alternatives->produce = true;
            require(decoder.receive(frame) == 0 && frame.buffer.format == V4L2_PIX_FMT_NV12 && frame.buffer.pitches[0] == alternatives->capture_pitch,
                    "driver's final answer, not alternative proposal, defines the export");
        }
        struct ExportCase {
            uint32_t width, height, pitch, bytes, bits;
            uint64_t uv, modifier;
        };
        for(const auto& item : {ExportCase{7296, 3648, 9728, 53497856, 10, 35663872, UINT64_C(0x0500000000000001)}, ExportCase{1937, 1081, 2624, 4300800, 10, 2859008, UINT64_C(0x0500000000000001)},
                                ExportCase{96, 64, 96, 9216, 8, 6144, 0}}) {
            auto exported = std::make_shared<QueueDevice>();
            exported->coded_format = V4L2_PIX_FMT_HEVC;
            exported->capture_format = item.bits == 10 ? v4l2_fourcc('Q', '1', '0', 'C') : V4L2_PIX_FMT_NV12;
            exported->capture_width = item.width;
            exported->capture_height = item.height;
            exported->capture_pitch = item.pitch;
            exported->capture_bytes = item.bytes;
            // Q10C is deliberately absent from enumeration, just like Iris.
            exported->offered_capture_formats = {V4L2_PIX_FMT_NV12, v4l2_fourcc('Q', '0', '8', 'C')};
            media::V4l2Decoder decoder(exported, {media::V4l2Codec::Hevc, item.width, item.height, item.bits});
            require(decoder.submit(packet, 1234567) == 0, "prime export descriptor fixture");
            media::V4l2Frame frame;
            require(decoder.receive(frame) == -EAGAIN, "wait for exported frame");
            require(exported->capture_requests.front().pixelformat == exported->capture_format, "request depth-selected fourcc directly even when not enumerated");
            if(item.bits == 10)
                require(std::count(exported->calls.begin(), exported->calls.end(), VIDIOC_ENUM_FMT) == 0, "Q10C bypasses raw format discovery");
            exported->produce = true;
            require(decoder.receive(frame) == 0, "documented compressed and linear frames reach exporter");
            Media playback{};
            playback.file = avformat_alloc_context();
            playback.frame = av_frame_alloc();
            require(playback.file && playback.frame, "allocate export adapter fixture");
            auto* video = avformat_new_stream(playback.file, nullptr);
            require(video != nullptr, "allocate export stream");
            video->time_base = {1, 1000000};
            video->codecpar->format = item.bits == 10 ? AV_PIX_FMT_YUV420P10LE : AV_PIX_FMT_YUV420P;
            media::export_v4l2_frame(playback, std::move(frame));
            const auto* drm = reinterpret_cast<const AVDRMFrameDescriptor*>(playback.frame->data[0]);
            require(drm->nb_objects == 1 && drm->nb_layers == 1 && drm->layers[0].nb_planes == 2 && drm->objects[0].format_modifier == item.modifier && drm->objects[0].size == item.bytes &&
                            drm->layers[0].format == (item.bits == 10 ? media::v4l2_p010 : V4L2_PIX_FMT_NV12),
                    "exporter keeps one object, two DRM planes, P010/QCOM modifier or linear NV12");
            for(int i = 0; i < 2; ++i)
                require(drm->layers[0].planes[i].object_index == 0 && drm->layers[0].planes[i].pitch == item.pitch && drm->layers[0].planes[i].offset == int64_t(i ? item.uv : 0),
                        "exporter preserves computed UV offset and driver pitch");
            char error[256]{};
            require(validate_dma_buf_frame(playback.frame, error, sizeof(error)) == 0, "modified DRM descriptor validates");
            media::FrameData described{};
            describe_frame(&described, playback.frame, &playback);
            const auto planes = lumaframevr::dma_buf_plane_layouts(described.dmabuf);
            require(described.dmabuf.modifier == item.modifier && described.dmabuf.source_layout && planes[0].offset == 0 && planes[1].offset == item.uv && planes[0].rowPitch == item.pitch &&
                            planes[1].rowPitch == item.pitch,
                    "final Vulkan import inputs keep the modifier and both explicit DRM planes");
        }
        for(uint32_t base : {0u, 4096u, 8192u}) {
            auto compressed = std::make_shared<QueueDevice>();
            compressed->capture_format = v4l2_fourcc('Q', '1', '0', 'C');
            compressed->capture_pitch = 256;
            compressed->capture_bytes = 32768;
            compressed->query_data_offset = base == 4096 ? base : 0;
            compressed->capture_data_offset = base == 8192 ? base : 0;
            compressed->capture_payload = base + 64;  // bytesused includes data_offset, not a linear image extent
            media::V4l2Decoder decoder(compressed, {media::V4l2Codec::H264, 96, 64, 10});
            decoder.submit(packet, 1234567);
            media::V4l2Frame frame;
            decoder.receive(frame);
            compressed->produce = true;
            require(decoder.receive(frame) == 0 && frame.buffer.offsets[0] == base && frame.buffer.offsets[1] == base + 20480 && frame.buffer.modifier == UINT64_C(0x0500000000000001),
                    "compressed payload uses allocation bounds and preserves reported memory-plane data offsets");
        }
        for(uint32_t bits : {8u, 10u})
            for(int fault = 0; fault < 5; ++fault) {
                auto payload = std::make_shared<QueueDevice>();
                if(bits == 10) {
                    payload->capture_format = v4l2_fourcc('Q', '1', '0', 'C');
                    payload->capture_pitch = 256;
                    payload->capture_bytes = 24576;
                }
                media::V4l2Decoder decoder(payload, {media::V4l2Codec::H264, 96, 64, bits});
                decoder.submit(packet, 1234567);
                media::V4l2Frame frame;
                decoder.receive(frame);
                payload->produce = true;
                if(fault == 0)
                    payload->capture_payload = 0;
                if(fault == 1)
                    payload->capture_payload = payload->capture_bytes + 1;
                if(fault == 2)
                    payload->capture_data_offset = payload->capture_bytes;
                if(fault == 3)
                    payload->capture_data_offset = 8192;  // shifted UV/full linear rows no longer fit
                if(fault == 4)
                    payload->capture_payload = 64;
                if(fault == 4 && bits == 10)
                    require(decoder.receive(frame) == 0, "short nonempty compressed payload is valid");
                else
                    rejects([&] { decoder.receive(frame); }, "invalid payload extent stays fail-closed for linear and compressed buffers");
            }
        for(int empty_error : {ENOENT, EAGAIN}) {
            auto empty = std::make_shared<QueueDevice>();
            empty->empty_event_error = empty_error;
            media::V4l2Decoder decoder(empty, stream);
            media::V4l2Frame frame;
            require(decoder.receive(frame) == -EAGAIN && empty->exports == 0, "empty event queue is not a decode failure");
            require(std::count(empty->calls.begin(), empty->calls.end(), VIDIOC_DQEVENT) == 0, "no event ioctl when poll reports no event");
            empty->stale_event_ready = true;
            require(decoder.receive(frame) == -EAGAIN && empty->exports == 0, "empty event queue after stale readiness is nonfatal");
            require(std::count(empty->calls.begin(), empty->calls.end(), VIDIOC_DQEVENT) == 1, "stale readiness exercises empty DQEVENT");
            require(decoder.submit(packet, 1234567) == 0, "empty event queue permits first input");
            require(decoder.receive(frame) == -EAGAIN && empty->exports == 8, "real source change after empty event queue configures capture");
            empty->produce = true;
            require(decoder.receive(frame) == 0, "decode continues after an empty event queue");
        }
        for(int unsupported : {EINVAL, ENOTTY, ENOSYS}) {
            auto no_events = std::make_shared<QueueDevice>();
            no_events->subscription_error = unsupported;
            try {
                media::V4l2Decoder decoder(no_events, stream);
                require(false, "stateful decoder cannot negotiate capture without source-change events");
            } catch(const std::system_error& error) {
                require(error.code().value() == unsupported && std::string(error.what()).find("subscribe source change") != std::string::npos,
                        "unsupported events retain subscription errno and diagnostic for fallback");
            }
            require(no_events->calls == std::vector<unsigned long>{VIDIOC_QUERYCAP, VIDIOC_SUBSCRIBE_EVENT}, "unsupported events rejected before polling or queue setup");
            require(no_events->maps == 0 && no_events->exports == 0, "unsupported events allocate nothing");
        }
        // Only COPY promises a stream clock. Exercise all mask encodings,
        // including reserved/vendor types, without inventing clock constants
        // absent from the Linux UAPI. Unrelated flags must not change the choice.
        for(uint32_t type = 0; type <= V4L2_BUF_FLAG_TIMESTAMP_MASK; type += 0x2000) {
            auto clocks = std::make_shared<QueueDevice>();
            clocks->timestamp = {8, 234567};
            clocks->separate_capture_timestamp = true;
            clocks->capture_timestamp = {15, 234567};  // seven-second capture-clock offset
            clocks->timestamp_flags = type | V4L2_BUF_FLAG_KEYFRAME | V4L2_BUF_FLAG_TSTAMP_SRC_SOE;
            media::V4l2Decoder decoder(clocks, stream);
            require(decoder.submit(packet, 8234567) == 0, "submit bitstream PTS for clock-type mapping");
            media::V4l2Frame frame;
            require(decoder.receive(frame) == -EAGAIN, "clock mapping waits for capture");
            clocks->produce = true;
            const int64_t expected = type == V4L2_BUF_FLAG_TIMESTAMP_COPY ? 15234567 : 8234567;
            require(decoder.receive(frame) == 0 && frame.timestamp_us == expected, "timestamp type " + std::to_string(type) + " selects COPY buffer PTS or bitstream PTS, never a capture clock");
        }
        for(bool copied : {false, true}) {
            auto invalid = std::make_shared<QueueDevice>();
            invalid->timestamp_flags = copied ? V4L2_BUF_FLAG_TIMESTAMP_COPY : V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
            invalid->separate_capture_timestamp = true;
            invalid->capture_timestamp = {15, 1000000};
            media::V4l2Decoder decoder(invalid, stream);
            require(decoder.submit(packet, 1234567) == 0, "submit PTS with unusable capture timeval");
            media::V4l2Frame frame;
            decoder.receive(frame);
            invalid->produce = true;
            if(copied)
                rejects([&] { decoder.receive(frame); }, "invalid COPY timeval is rejected");
            else
                require(decoder.receive(frame) == 0 && frame.timestamp_us == 1234567, "capture clock is ignored, not validated as a stream PTS");
        }
        {
            auto reordered = std::make_shared<QueueDevice>();
            reordered->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
            reordered->separate_capture_timestamp = true;
            reordered->capture_timestamp = {15, 234567};
            media::V4l2Decoder decoder(reordered, stream);
            // I, P, B input order; output storage can be recycled before frames.
            reordered->timestamp = {8, 234567};
            require(decoder.submit(packet, 8234567) == 0, "submit I-frame AU PTS");
            reordered->timestamp = {9, 34567};
            require(decoder.submit(packet, 9034567) == 0, "submit future P-frame AU PTS");
            reordered->timestamp = {8, 634567};
            require(decoder.submit(packet, 8634567) == -EAGAIN, "backpressure does not retain an unsubmitted PTS");
            reordered->output_done = true;
            require(decoder.submit(packet, 8634567) == 0, "reused OUTPUT slot retains B-frame AU PTS");
            require(decoder.drain() == 0, "drain retains pending AU timestamps");
            for(int64_t expected : {8234567, 8634567, 9034567}) {
                media::V4l2Frame frame;
                reordered->produce = true;
                require(decoder.receive(frame) == 0 && frame.timestamp_us == expected, "capture-clock frames use AU PTS in presentation order, not submission order");
            }
            reordered->produce = reordered->last = reordered->empty_last = true;
            media::V4l2Frame frame;
            require(decoder.receive(frame) == AVERROR_EOF && !frame.owner, "empty LAST does not require or consume an AU timestamp");
        }
        struct TimestampCase {
            int64_t ticks, start_ticks, microseconds;
            int denominator;
            timeval kernel;
            double relative_seconds;
        };
        const TimestampCase timestamps[]{
                {741000, 630000, 8233333, 90000, {8, 233333}, 1.2333333333333334},
                {-22500, 0, -250000, 90000, {-1, 750000}, -0.25},
                {8234, 7000, 8234000, 1000, {8, 234000}, 1.234},
                {8234567, 7000000, 8234567, 1000000, {8, 234567}, 1.234567},
        };
        for(const auto& item : timestamps) {
            auto ten = std::make_shared<QueueDevice>();
            ten->coded_format = V4L2_PIX_FMT_HEVC;
            ten->capture_format = v4l2_fourcc('Q', '1', '0', 'C');
            ten->capture_width = 128;
            ten->capture_height = 80;
            ten->capture_pitch = 320;
            ten->capture_bytes = 38400;
            ten->export_bytes = 42496;  // tail padding must not move the UV offset
            ten->timestamp = item.kernel;
            media::V4l2Decoder decoder(ten, {media::V4l2Codec::Hevc, 96, 64, 10});
            const AVRational time_base{1, item.denominator};
            const auto microseconds = av_rescale_q(item.ticks, time_base, AV_TIME_BASE_Q);
            require(microseconds == item.microseconds, "stream PTS converts to fixed microseconds without subtracting origin");
            require(decoder.submit(packet, microseconds) == 0, "Q10C stream packet timestamp reaches output timeval");
            media::V4l2Frame frame;
            require(decoder.receive(frame) == -EAGAIN && ten->capture_readback, "Q10C capture uses post-S_FMT G_FMT before allocating");
            ten->produce = true;
            require(decoder.receive(frame) == 0 && frame.timestamp_us == item.microseconds, "capture timeval reconstructs presentation timestamp including negative PTS");
            const auto generation = frame.buffer.pool;
            require(frame.buffer.source_layout && frame.buffer.format == media::v4l2_p010 && frame.buffer.width == 128 && frame.buffer.height == 80 && frame.buffer.size == 42496 &&
                            std::to_array(frame.buffer.pitches) == std::array<uint64_t, 2>{320, 320} && std::to_array(frame.buffer.offsets) == std::array<uint64_t, 2>{0, 32768},
                    "modified P010 descriptor preserves driver pitch and aligned Q10C metadata/luma offset");
            Media playback{};
            playback.file = avformat_alloc_context();
            playback.frame = av_frame_alloc();
            require(playback.file && playback.frame, "allocate real DRM adapter fixture");
            auto* video = avformat_new_stream(playback.file, nullptr);
            require(video != nullptr, "allocate timestamp stream");
            video->time_base = time_base;
            video->start_time = item.start_ticks;
            video->codecpar->format = AV_PIX_FMT_YUV420P10LE;
            playback.time_base = av_q2d(time_base);
            playback.start = item.start_ticks * playback.time_base;
            media::export_v4l2_frame(playback, std::move(frame));
            require(playback.frame->opaque != nullptr && media::v4l2_source_layout(*playback.frame), "V4L2 layout provenance survives the DRM adapter");
            auto* cloned = av_frame_clone(playback.frame);
            require(cloned && media::v4l2_source_layout(*cloned), "retained DRM frame keeps source layout provenance");
            require(playback.frame->pts == item.ticks && playback.frame->best_effort_timestamp == item.ticks, "DRM adapter converts microseconds back to original stream ticks");
            near(playback.frame->best_effort_timestamp * playback.time_base - playback.start, item.relative_seconds, 1e-9, "presentation clock subtracts stream origin once, not twice");
            auto* drm = reinterpret_cast<const AVDRMFrameDescriptor*>(playback.frame->data[0]);
            require(drm->nb_objects == 1 && drm->nb_layers == 1 && drm->layers[0].nb_planes == 2 && drm->layers[0].format == media::v4l2_p010 && drm->objects[0].size == 42496,
                    "one contiguous V4L2 memory plane exports two DRM color planes");
            char error[256]{};
            require(validate_dma_buf_frame(playback.frame, error, sizeof(error)) == 0, "real DRM descriptor validates");
            media::FrameData described{};
            describe_frame(&described, cloned, &playback);
            require(described.dmabuf.fd == 100 && described.dmabuf.pool == generation && std::to_array(described.dmabuf.pitches) == std::array<uint64_t, 2>{320, 320} &&
                            std::to_array(described.dmabuf.offsets) == std::array<uint64_t, 2>{0, 32768} && described.dmabuf.width == 128 && described.dmabuf.height == 80 &&
                            std::to_array(described.dmabuf.crop) == std::array<int, 4>{0, 0, 32, 16},
                    "descriptor reaching Vulkan preserves queue layout and crop without recomputation");
            require(described.dmabuf.source_layout, "negotiated V4L2 layout is authoritative at Vulkan import");
            const auto planes = lumaframevr::dma_buf_plane_layouts(described.dmabuf);
            require(planes[0].offset == 0 && planes[0].rowPitch == 320 && planes[1].offset == 32768 && planes[1].rowPitch == 320,
                    "Vulkan explicit P010 planes use source pitch and aligned Q10C UV offset");
            require(lumaframevr::dma_buf_size_action(described.dmabuf, 42497) == lumaframevr::DmaBufSizeAction::Reject, "P010 size retry must not replace an authoritative layout with a derived one");
            const auto after_retry = lumaframevr::dma_buf_plane_layouts(described.dmabuf);
            require(after_retry[0].offset == planes[0].offset && after_retry[0].rowPitch == planes[0].rowPitch && after_retry[1].offset == planes[1].offset &&
                            after_retry[1].rowPitch == planes[1].rowPitch,
                    "P010 source plane offsets and pitches survive the rejected size retry");
            require(lumaframevr::dma_buf_size_action(described.dmabuf, 42496) == lumaframevr::DmaBufSizeAction::Import &&
                            lumaframevr::dma_buf_size_action(described.dmabuf, 38400) == lumaframevr::DmaBufSizeAction::Import,
                    "authoritative P010 exact-size and padded allocations can be imported");
            require(lumaframevr::dma_buf_size_action(described.dmabuf, 42497, true) == lumaframevr::DmaBufSizeAction::Reject, "an undersized authoritative allocation is never bound");
            auto computed = described.dmabuf;
            computed.source_layout = false;
            require(lumaframevr::dma_buf_size_action(computed, 42497) == lumaframevr::DmaBufSizeAction::DeriveLayout, "computed layout mismatch retains the existing derived-layout retry");
            require(lumaframevr::dma_buf_size_action(computed, 42496, true) == lumaframevr::DmaBufSizeAction::Import, "computed retry can import a fitting Vulkan-derived layout");
            require(lumaframevr::dma_buf_size_action(computed, 42497, true) == lumaframevr::DmaBufSizeAction::Reject, "computed retry still refuses an allocation smaller than Vulkan requirements");
            require(lumaframevr::dma_buf_format(described.dmabuf.format, 10) == VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, "P010 DRM FourCC selects Vulkan two-plane 10-bit packing");
            // Unmarked DRM frames keep the old import policy, even while a V4L2 lease exists.
            cloned->opaque = nullptr;
            media::FrameData unmarked{};
            describe_frame(&unmarked, cloned, &playback);
            require(!unmarked.dmabuf.source_layout, "unmarked DRM layout keeps legacy retry policy");
            av_frame_unref(cloned);
            require(!media::v4l2_source_layout(*cloned), "reused AVFrame does not inherit source layout provenance");
            av_frame_free(&cloned);
        }
        auto device = std::make_shared<QueueDevice>();
        media::V4l2Frame held;
        {
            media::V4l2Decoder decoder(device, stream);
            require(device->exports == 0, "capture not allocated before source change");
            require(decoder.submit(packet, 1234567) == 0, "submit access unit");
            require(decoder.receive(held) == -EAGAIN && !held.owner, "nonblocking capture waits");
            require(device->exports == 8 && device->capture_on, "source change allocates and exports capture pool");
            device->produce = true;
            require(decoder.receive(held) == 0 && held.owner, "dequeued frame has lifetime owner");
            require(held.timestamp_us == 1234567, "capture timestamp preserved");
            require(held.buffer.fd == 100 && held.buffer.format == V4L2_PIX_FMT_NV12 && held.buffer.size == 9216, "existing DMA-BUF descriptor");
            require(held.buffer.offsets[1] == 6144 && held.buffer.pitches[1] == 96 && held.buffer.width == 96 && held.buffer.height == 64, "driver layout reaches consumer");
            require(device->capture_queued == 8, "held frame is not prematurely requeued");
            auto copy = held;
            held = {};
            media::V4l2Frame empty;
            require(decoder.receive(empty) == -EAGAIN && device->capture_queued == 8, "copy retains capture buffer");
            copy = {};
            require(decoder.receive(empty) == -EAGAIN && device->capture_queued == 9, "last release requeues on decode thread");
            require(decoder.submit(packet, 1234567) == 0, "second output slot available");
            require(decoder.submit(packet, 1234567) == -EAGAIN, "output backpressure retains packet");
            device->output_done = true;
            require(decoder.submit(packet, 1234567) == 0, "dequeued output reused");
            require(decoder.drain() == 0 && device->stopped, "drain requests decoder STOP");
            device->produce = true;
            device->last = true;
            require(decoder.receive(held) == 0 && held.owner, "LAST with payload is still a frame");
            require(decoder.receive(empty) == AVERROR_EOF, "next receive reports drained EOF");
            require(decoder.submit(packet, 1234567) == AVERROR_EOF, "no input after drain");
        }
        require(!device->output_on && !device->capture_on && device->unmaps == 2, "decoder stops queues and unmaps coded buffers");
        require(device->closes == 0 && held.buffer.fd == 100, "retained export survives decoder destruction");
        held = {};
        require(device->closes == device->exports, "all exported fds closed exactly once after last frame");
        // A replacement decoder is built while the previous queue's retained
        // frames are still leasing its buffers, which is the ENOMEM a seek or a
        // later open hits. These three cases prove the ordering: a live lease
        // blocks allocation until it retires, a lease that never retires is
        // reported against its budget without being invalidated, and an already
        // retired lease does not block at all.
        {
            ScopedEnvironment budget("LUMAFRAMEVR_LEASE_WAIT_MS", "8000");  // generous: the frame retires mid-wait
            auto previous = std::make_shared<QueueDevice>();
            media::V4l2Frame held;
            {
                media::V4l2Decoder decoder(previous, stream);
                decoder.submit(packet, 1234567);
                media::V4l2Frame empty;
                decoder.receive(empty);
                previous->produce = true;
                require(decoder.receive(held) == 0 && held.owner, "replacement fixture retains a leased frame");
            }
            require(previous->closes == 0 && previous->exports == 8, "a retained frame keeps the retired queue's buffers allocated");
            require(media::outstanding_capture_leases() == 1 && media::outstanding_capture_queues() == 1, "the retired frame and its unfreed queue are visible before the replacement allocates");
            auto replacement = std::make_shared<QueueDevice>();
            replacement->allocation_probe = buffer_probe();
            std::atomic<bool> retired{false};
            std::jthread consumer([&] {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                held = {};
                retired = true;
            });
            const auto started = std::chrono::steady_clock::now();
            {
                media::V4l2Decoder decoder(replacement, stream);
                require(decoder.submit(packet, 1234567) == 0, "the ordered replacement decodes once it has allocated");
            }
            const auto waited = std::chrono::steady_clock::now() - started;
            consumer.join();
            require(retired.load() && !replacement->allocated_while_leased, "a live lease blocks the replacement until the old queue releases its buffers");
            require(waited >= std::chrono::milliseconds(20) && waited < std::chrono::milliseconds(4000), "the bounded wait returns when the lease retires, not when its budget expires");
            require(previous->closes == previous->exports && previous->maps == previous->unmaps, "the retired queue closes its exports and releases both allocations once the last frame goes");
            require(media::outstanding_capture_leases() == 0 && media::outstanding_capture_queues() == 0, "no frame lease or queue is left behind after retirement");
        }
        {
            ScopedEnvironment budget("LUMAFRAMEVR_LEASE_WAIT_MS", "5");  // shorter than any consumer can answer
            auto previous = std::make_shared<QueueDevice>();
            media::V4l2Frame held;
            {
                media::V4l2Decoder decoder(previous, stream);
                decoder.submit(packet, 1234567);
                media::V4l2Frame empty;
                decoder.receive(empty);
                previous->produce = true;
                require(decoder.receive(held) == 0, "timeout fixture retains a leased frame");
            }
            auto replacement = std::make_shared<QueueDevice>();
            replacement->allocation_probe = buffer_probe();
            std::string diagnostic;
            const auto started = std::chrono::steady_clock::now();
            {
                std::FILE* log = std::fopen("v4l2-lease-timeout.log", "w+");
                require(log != nullptr, "open lease timeout diagnostic capture");
                const int saved = dup(STDERR_FILENO);
                require(saved >= 0, "save stderr for the lease timeout diagnostic");
                std::fflush(stderr);
                require(dup2(fileno(log), STDERR_FILENO) >= 0, "capture the lease timeout diagnostic");
                {
                    media::V4l2Decoder stalled(replacement, stream);
                    require(stalled.submit(packet, 1234567) == 0, "the replacement allocates after the timeout instead of stalling");
                }
                std::fflush(stderr);
                require(dup2(saved, STDERR_FILENO) >= 0, "restore stderr after the lease timeout");
                close(saved);
                std::rewind(log);
                char bytes[1024];
                while(const auto count = std::fread(bytes, 1, sizeof(bytes), log)) diagnostic.append(bytes, count);
                std::fclose(log);
            }
            const auto waited = std::chrono::steady_clock::now() - started;
            require(diagnostic.find("did not retire within 5 ms") != std::string::npos && diagnostic.find("REQBUFS may fail with ENOMEM") != std::string::npos,
                    "a lease that never retires is reported against the budget it exceeded");
            require(waited >= std::chrono::milliseconds(5) && waited < std::chrono::milliseconds(1000), "the wait is bounded by its budget, not by the consumer");
            require(replacement->allocation_seen && replacement->allocated_while_leased, "a timeout allocates anyway instead of stalling playback");
            require(media::outstanding_capture_leases() == 1 && previous->closes == 0, "a stuck lease is reported, not invalidated or freed underneath its consumer");
            held = {};
            require(previous->closes == previous->exports, "the stuck queue frees its buffers when its frame finally goes");
            require(media::outstanding_capture_leases() == 0, "the timeout leaves no lease behind");
        }
        {
            ScopedEnvironment budget("LUMAFRAMEVR_LEASE_WAIT_MS", "8000");  // a blocking wait here would show up as a stall
            auto released = std::make_shared<QueueDevice>();
            media::V4l2Frame held;
            {
                media::V4l2Decoder decoder(released, stream);
                decoder.submit(packet, 1234567);
                media::V4l2Frame empty;
                decoder.receive(empty);
                released->produce = true;
                require(decoder.receive(held) == 0, "fast-path fixture retains a leased frame");
            }
            held = {};
            require(media::outstanding_capture_leases() == 0 && media::outstanding_capture_queues() == 0, "a retired frame and its released queue leave nothing to wait for");
            const auto started = std::chrono::steady_clock::now();
            require(media::await_capture_leases(), "an unleased queue needs no wait");
            auto replacement = std::make_shared<QueueDevice>();
            replacement->allocation_probe = buffer_probe();
            media::V4l2Decoder decoder(replacement, stream);
            require(!replacement->allocated_while_leased, "a retired lease does not block the replacement");
            require(std::chrono::steady_clock::now() - started < std::chrono::milliseconds(250), "an already retired lease costs the replacement no budget");
        }
        // A refused allocation has to say which queue it was and whether the
        // device can allocate at all: on the frame, "Cannot allocate memory" from
        // a coded queue and from a 424 MB capture pool look identical otherwise.
        {
            auto refused = std::make_shared<QueueDevice>();
            refused->enomem_above = 1;  // the device grants one buffer and refuses two
            std::string detail;
            try {
                media::V4l2Decoder(refused, stream);
            } catch(const std::exception& error) {
                detail = error.what();
            }
            require(!detail.empty(), "a refused coded allocation cannot open a decoder");
            require(detail.find("coded REQBUFS 2 buffer(s)") != std::string::npos && detail.find("refused 3 times over 500 ms") != std::string::npos &&
                            detail.find("a single buffer is granted") != std::string::npos && detail.find("0 retained frame lease(s), 0 capture queue(s) outstanding") != std::string::npos &&
                            detail.find("1 decoder(s) open here (including this one)") != std::string::npos,
                    "a refused allocation names the queue, how often it was refused, the frame leases outstanding, this process's own decoder and the smallest request the device grants");
            require(refused->buffer_requests.size() == 5 && refused->buffer_requests[0].count == 2 && refused->buffer_requests[1].count == 2 && refused->buffer_requests[2].count == 2 &&
                            refused->buffer_requests[3].count == 1 && refused->buffer_requests[4].count == 0,
                    "a request that stays refused is attempted three times, then probed with a single buffer and released again");
            require(refused->maps == 0 && refused->exports == 0 && refused->closes == refused->exports, "a refused coded allocation leaves no map or export behind");
        }
        // A refusal that clears is worth waiting a quarter of a second for: the
        // VPU's memory is device wide, so the client holding it can let go.
        {
            auto clearing = std::make_shared<QueueDevice>();
            clearing->enomem_times = 1;  // refused once, granted on the retry
            {
                media::V4l2Decoder decoder(clearing, stream);
                require(decoder.submit(packet, 1234567) == 0, "a cleared refusal leaves a usable decoder");
            }
            require(clearing->enomem_refusals == 1 && clearing->buffer_requests.size() == 3 && clearing->buffer_requests[0].count == 2 && clearing->buffer_requests[1].count == 2 &&
                            clearing->buffer_requests[2].count == 0,
                    "a refused allocation is retried unchanged before the file gives up on the VPU");
            require(clearing->maps == 2 && clearing->unmaps == 2, "the retried coded queue is mapped and released once");
        }
        // "Ours or someone else's?" has to be answerable from the log: the refusal
        // reports how many decoders this process still holds, and who else holds
        // the nodes it tried.
        {
            auto counted = std::make_shared<QueueDevice>();
            std::string detail;
            {
                media::V4l2Decoder decoder(counted, stream);
                require(media::open_decoders() == 1, "an open decoder is counted as this process's own");
                auto refused = std::make_shared<QueueDevice>();
                refused->enomem_above = 1;
                try {
                    media::V4l2Decoder(refused, stream);
                } catch(const std::exception& error) {
                    detail = error.what();
                }
            }
            require(detail.find("2 decoder(s) open here (including this one)") != std::string::npos, "a refusal says how many decoders this process still holds");
            require(media::open_decoders() == 0, "decoders are uncounted again once they are gone");
        }
        {
            // The holder report reads /proc; a fixture file stands in for a node,
            // because this host has no VPU and the mechanism is what is under test.
            const std::string fixture = "v4l2-holder-fixture";
            std::FILE* node = std::fopen(fixture.c_str(), "w+");
            require(node != nullptr, "create the holder-report fixture");
            std::fclose(node);
            const auto cwd = std::filesystem::current_path();
            const std::string absolute = (cwd / fixture).string();
            require(media::vpu_holder_report({absolute}).empty(), "an unheld node names no holder");
            std::FILE* held = std::fopen(absolute.c_str(), "r");
            require(held != nullptr, "hold the fixture the way a decoder node would be held");
            const std::string report = media::vpu_holder_report({absolute});
            require(report.find(absolute) != std::string::npos && report.find(std::to_string(getpid())) != std::string::npos, "a held node names the process holding it");
            require(media::vpu_holder_report({absolute}).find(" x") == std::string::npos, "a node held once reports no count");
            // The count is what tells a decoder being opened from one that was
            // never closed: this process always holds the node it is opening.
            std::FILE* again = std::fopen(absolute.c_str(), "r");
            std::FILE* third = std::fopen(absolute.c_str(), "r");
            require(again != nullptr && third != nullptr, "hold the fixture twice");
            const std::string counted = media::vpu_holder_report({absolute});
            require(counted.find("[this process]") != std::string::npos && counted.find(" x3") != std::string::npos, "a node held more than once reports how many descriptors this process has on it");
            std::fclose(again);
            std::fclose(third);
            std::fclose(held);
            require(media::vpu_holder_report({absolute}).empty(), "the holder report forgets every released descriptor");
            std::filesystem::remove(fixture);
        }
        // The capture headroom is overridable so a refused pool can be tested at
        // a smaller request on the device without another build; the driver's
        // answer still decides the queue.
        {
            ScopedEnvironment headroom("LUMAFRAMEVR_V4L2_CAPTURE_HEADROOM", "2");
            auto smaller = std::make_shared<QueueDevice>();
            smaller->expected_headroom = 2;
            media::V4l2Decoder decoder(smaller, stream);
            decoder.submit(packet, 1234567);
            media::V4l2Frame frame;
            require(decoder.receive(frame) == -EAGAIN && smaller->exports == 8 && smaller->capture_on, "a headroom override still allocates and streams the capture pool");
            require(smaller->buffer_requests.size() == 3 && smaller->buffer_requests[2].count == smaller->dmabuf_granted + 2,
                    "capture headroom changes the request only, not the count the driver grants");
        }
        for(int fault = 0; fault < 6; ++fault) {
            auto bad = std::make_shared<QueueDevice>();
            bad->short_buffer = fault == 0;
            bad->short_export = fault == 1;
            bad->wrong_format = fault == 2;
            if(fault == 3)
                bad->capture_granted = 0;
            if(fault == 4)
                bad->fail_request = VIDIOC_QBUF;
            bad->map_failed = fault == 5;
            rejects(
                    [&] {
                        media::V4l2Decoder decoder(bad, stream);
                        decoder.submit(packet, 1234567);
                        media::V4l2Frame frame;
                        decoder.receive(frame);
                    },
                    "unsafe negotiation or queue failure rejected");
            require(bad->closes == bad->exports && bad->unmaps == bad->maps, "partial initialization releases resources");
        }
        if(const char* path = std::getenv("LUMAFRAMEVR_TEST_VIDEO")) {
            auto opened = media::open(path, true);
            require(std::holds_alternative<media::Session>(opened), "startup retry demux fixture opens");
            auto session = std::move(std::get<media::Session>(opened));
            auto& playback = *session.get();
            auto first = std::make_shared<QueueDevice>(), second = std::make_shared<QueueDevice>();
            int attempts = 0;
            playback.v4l2 = std::make_unique<media::V4l2Playback>(playback, [&](media::V4l2Stream spec) {
                auto current = attempts++ == 0 ? first : second;
                if(current == second)
                    require(first->closes == first->exports && first->maps == first->unmaps, "failed startup queue is fully retired before reopen");
                auto decoder = std::make_unique<media::V4l2Decoder>(current, spec);
                decoder->submit(packet, 1234567);
                media::V4l2Frame empty;
                decoder->receive(empty);
                current->produce = true;
                current->error_frame = current == first;
                return decoder;
            });
            playback.skip_hardware_decoder = playback.software_decoding = 0;
            // The retry path rewinds and then builds a replacement decoder while a
            // previous queue can still be leased by a frame the consumer has not
            // retired. Hold exactly such a lease on a foreign queue and keep it
            // leased past every budget: the retry must report both of its waits -
            // one before the rewind, one before the replacement allocates - and
            // still complete, rather than stalling or failing on the lease.
            ScopedEnvironment budget("LUMAFRAMEVR_LEASE_WAIT_MS", "5");
            auto foreign = std::make_shared<QueueDevice>();
            media::V4l2Frame held;
            {
                media::V4l2Decoder leased(foreign, stream);
                leased.submit(packet, 1234567);
                media::V4l2Frame empty;
                leased.receive(empty);
                foreign->produce = true;
                require(leased.receive(held) == 0 && held.owner, "retry fixture holds a foreign leased queue");
            }
            second->allocation_probe = buffer_probe();
            std::string diagnostic;
            int status = -1;
            {
                std::FILE* log = std::fopen("v4l2-retry-lease.log", "w+");
                require(log != nullptr, "open retry lease diagnostic capture");
                const int saved = dup(STDERR_FILENO);
                require(saved >= 0, "save stderr for the retry lease diagnostic");
                std::fflush(stderr);
                require(dup2(fileno(log), STDERR_FILENO) >= 0, "capture the retry lease diagnostic");
                status = media::receive(playback).status;
                std::fflush(stderr);
                require(dup2(saved, STDERR_FILENO) >= 0, "restore stderr after the retry lease diagnostic");
                close(saved);
                std::rewind(log);
                char bytes[1024];
                while(const auto count = std::fread(bytes, 1, sizeof(bytes), log)) diagnostic.append(bytes, count);
                std::fclose(log);
            }
            require(status == 1 && playback.v4l2 && attempts == 2 && !playback.software_decoding, "first capture EIO retries once on a fresh device before software fallback");
            require(count_of(diagnostic, "did not retire within 5 ms") == 2 && diagnostic.find("1 retained frame lease(s)") != std::string::npos,
                    "the startup retry waits for a leased queue before its rewind and again before its replacement allocates");
            require(second->allocation_seen && second->allocated_while_leased, "a lease that never retires is reported against its budget, not fatal");
            held = {};
            require(foreign->closes == foreign->exports, "the foreign queue frees its buffers once its frame goes");
            second->produce = second->error_frame = true;
            second->capture_index = 1;
            require(media::receive(playback).status >= 0 && !playback.v4l2 && playback.software_decoding && attempts == 2,
                    "capture EIO after a delivered frame is a genuine failure, not a reopen loop");
        }
        if(const char* path = std::getenv("LUMAFRAMEVR_TEST_VIDEO"))
            for(int error : {EIO, EBUSY, ENOTSUP}) {
                auto opened = media::open(path, true);
                require(std::holds_alternative<media::Session>(opened), "persistent startup failure fixture opens");
                auto session = std::move(std::get<media::Session>(opened));
                auto& playback = *session.get();
                std::vector<std::shared_ptr<QueueDevice>> devices;
                playback.v4l2 = std::make_unique<media::V4l2Playback>(playback, [&](media::V4l2Stream spec) {
                    auto current = std::make_shared<QueueDevice>();
                    devices.push_back(current);
                    auto decoder = std::make_unique<media::V4l2Decoder>(current, spec);
                    decoder->submit(packet, 1234567);
                    media::V4l2Frame empty;
                    decoder->receive(empty);
                    if(error == EIO)
                        current->produce = current->error_frame = true;
                    else
                        current->poll_error = error;
                    return decoder;
                });
                playback.skip_hardware_decoder = playback.software_decoding = 0;
                require(media::receive(playback).status >= 0 && !playback.v4l2 && playback.software_decoding && devices.size() == (error == ENOTSUP ? 1u : 2u),
                        "persistent startup EIO/EBUSY exhausts one retry; nontransient errors fall back immediately");
                for(const auto& current : devices)
                    require(current->maps == current->unmaps && current->exports == current->closes, "all failed startup sessions balance resources before software playback");
                media::Frame cpu;
                int result;
                do {
                    result = session.next(cpu).status;
                } while(result == 2 || result == 3);
                require(result == 1 && cpu.get().software, "automatic fallback really decodes the local fixture with FFmpeg");
            }
        if(const char* path = std::getenv("LUMAFRAMEVR_TEST_VIDEO")) {
            auto opened = media::open(path, true);
            require(std::holds_alternative<media::Session>(opened), "V4L2 seek adapter opens host demux fixture");
            auto session = std::move(std::get<media::Session>(opened));
            auto& playback = *session.get();
            auto seek = std::make_shared<QueueDevice>();
            playback.v4l2 = std::make_unique<media::V4l2Playback>(playback, [seek](media::V4l2Stream spec) {
                auto decoder = std::make_unique<media::V4l2Decoder>(seek, spec);
                decoder->submit(packet, 1234567);
                media::V4l2Frame empty;
                decoder->receive(empty);
                return decoder;
            });
            playback.skip_hardware_decoder = playback.software_decoding = 0;
            require(session.seek(0.25).status == 0 && playback.v4l2 && !playback.skip_hardware_decoder && seek->closes == 0 && seek->maps == 2 && seek->exports == 8 && seek->capture_on &&
                            seek->output_on,
                    "a real session seek leaves both queues streaming: nothing closed, nothing reallocated, nothing stopped and nothing demoted");
        }
        if(const char* path = std::getenv("LUMAFRAMEVR_TEST_VIDEO")) {
            // A frame the player decoded before the seek is still leased across it.
            // The flush releases nothing, so there is nothing to wait for either: the
            // unleased slots are requeued, the leased one when its consumer retires
            // it, and the file must not be demoted to software for any of it.
            auto opened = media::open(path, true);
            require(std::holds_alternative<media::Session>(opened), "leased seek fixture opens host demux fixture");
            auto session = std::move(std::get<media::Session>(opened));
            auto& playback = *session.get();
            auto seek = std::make_shared<QueueDevice>();
            media::V4l2Frame held;
            playback.v4l2 = std::make_unique<media::V4l2Playback>(playback, [seek, &held](media::V4l2Stream spec) {
                auto decoder = std::make_unique<media::V4l2Decoder>(seek, spec);
                decoder->submit(packet, 1234567);
                media::V4l2Frame empty;
                decoder->receive(empty);
                seek->produce = true;
                require(decoder->receive(held) == 0 && held.owner, "leased seek fixture retains a frame");
                return decoder;
            });
            playback.skip_hardware_decoder = playback.software_decoding = 0;
            require(media::outstanding_capture_leases() == 1, "the seek's own frame is leased before it starts");
            const auto started = std::chrono::steady_clock::now();
            require(session.seek(0.25).status == 0 && playback.v4l2 && !playback.skip_hardware_decoder, "a seek flushes even while a frame it decoded is still leased");
            require(media::outstanding_capture_leases() == 1 && seek->closes == 0 && seek->exports == 8 && seek->capture_on, "a seek neither releases nor invalidates the frame it handed out");
            require(std::chrono::steady_clock::now() - started < std::chrono::milliseconds(250), "a seek does not wait for the consumer: it has nothing to release");
            held = {};
            require(media::outstanding_capture_leases() == 0, "the frame is the consumer's to retire, and the seek left it whole");
        }
        {
            // The default: a seek issues no device ioctl at all. Stopping the queues
            // is what this driver refuses - "capture STREAMON: Cannot allocate
            // memory" with its own minimum pool and every slot back - so the queues
            // stay streaming and only our own state is dropped. Frames the driver
            // still emits for the old segment carry pre-seek timestamps and are
            // discarded by the seek target.
            auto seek = std::make_shared<QueueDevice>();
            media::V4l2Decoder decoder(seek, stream);
            decoder.submit(packet, 1234567);
            media::V4l2Frame old, empty;
            decoder.receive(empty);
            seek->produce = true;
            require(decoder.receive(old) == 0, "default seek fixture retains a frame");
            const auto generation = old.buffer.pool;
            seek->calls.clear();
            seek->lifecycle.clear();
            require(decoder.reset() == 0 && seek->capture_on && seek->output_on, "a seek leaves both queues streaming; it stops them only when the restart knob asks");
            require(std::count_if(seek->calls.begin(), seek->calls.end(), [](unsigned long call) { return call != QueueDevice::poll_call; }) == 0 && seek->lifecycle.empty(),
                    "a seek issues no ioctl at all: no REQBUFS, no STREAMOFF, no STREAMON, no QBUF and no export");
            require(seek->closes == 0 && seek->exports == 8 && seek->maps == 2, "a seek closes no export and maps nothing");
            require(decoder.submit(packet, 1234567) == 0, "the coded queue keeps taking input across a seek");
            // Slot 0 still belongs to the frame this test holds; the driver hands out
            // the slots that are back in its queue.
            seek->capture_index = 1;
            seek->produce = true;
            require(decoder.receive(old) == 0 && old.buffer.pool == generation, "frames after a seek come from the pool the driver kept, with no renegotiation");
            // A discontinuity can cost the driver its references for the old segment,
            // per buffer. Those frames are dropped - bounded, and only after a seek.
            seek->capture_index = 2;  // slot 1 is spoken for by the frame above
            seek->produce = seek->error_frame = true;
            require(decoder.receive(empty) == -EAGAIN, "a decode error right after a seek is dropped, not fatal");
            seek->error_frame = false;
            seek->produce = true;
            require(decoder.receive(old) == 0, "the same queues keep decoding once frames are good again");
            require(seek->closes == 0 && media::outstanding_capture_leases() == 1, "the dropped frame's slot was reclaimed, and nothing was closed or written off");
            old = {};
        }
        for(bool drained : {false, true}) {
            // These are the stop-and-restart cases: the device refuses that restart,
            // so it is no longer the default and lives behind the comparison knob.
            ScopedEnvironment restart("LUMAFRAMEVR_V4L2_SEEK_RESTART", "1");
            auto seek = std::make_shared<QueueDevice>();
            media::V4l2Decoder decoder(seek, stream);
            decoder.submit(packet, 1234567);
            media::V4l2Frame old, empty;
            decoder.receive(empty);
            seek->produce = true;
            require(decoder.receive(old) == 0, "seek fixture retains old pool");
            const auto generation = old.buffer.pool;
            if(drained) {
                decoder.drain();
                seek->capture_index = 1;  // LAST must not dequeue the retained slot
                seek->produce = seek->last = seek->empty_last = true;
                require(decoder.receive(empty) == AVERROR_EOF, "seek fixture reaches drained EOF");
                seek->capture_index = 0;
            }
            seek->last = seek->empty_last = false;
            const int exports_before = seek->exports, maps_before = seek->maps, queued_before = seek->capture_queued;
            seek->lifecycle.clear();
            seek->calls.clear();
            // This replaced the teardown: a seek used to stop both queues, wait for
            // the retained frame, close every export, free both allocations and
            // rebuild the coded queue. The device answered the coded REQBUFS that
            // followed with three ENOMEMs over 500 ms and the file fell back to
            // software for the rest of the session. A seek is now a flush of the
            // queues the driver already holds - nothing is released or re-negotiated,
            // so there is no allocation left to refuse, and the 8K capture pool is
            // not re-negotiated on every seek.
            require(decoder.reset() == 0 && seek->capture_on && !seek->output_on, "a seek flushes both queues and restarts capture, releasing nothing");
            require(std::count(seek->calls.begin(), seek->calls.end(), VIDIOC_REQBUFS) == 0 && std::count(seek->calls.begin(), seek->calls.end(), VIDIOC_EXPBUF) == 0 &&
                            std::count(seek->calls.begin(), seek->calls.end(), VIDIOC_S_FMT) == 0 && std::count(seek->calls.begin(), seek->calls.end(), VIDIOC_G_FMT) == 0 &&
                            std::count(seek->calls.begin(), seek->calls.end(), VIDIOC_STREAMOFF) == 2 && std::count(seek->calls.begin(), seek->calls.end(), VIDIOC_STREAMON) == 1,
                    "a seek issues no REQBUFS at all: no coded re-allocation, no release, no export and no format negotiation");
            require(seek->lifecycle == std::vector<std::string>{"capture-off", "output-off"}, "a seek stops both queues in that order and asks the driver for nothing else");
            require(seek->exports == exports_before && seek->maps == maps_before && seek->closes == 0, "a seek keeps both allocations: no export, no mapping and no close");
            require(seek->capture_queued - queued_before == int(seek->capture_granted) - 1, "a seek requeues every capture slot except the one a frame still leases");
            // The frame kept from before the seek is still leased, and its slot goes
            // back only when the consumer retires it - on the decode thread, because
            // that is where every queue ioctl belongs.
            old = {};
            seek->produce = true;
            require(decoder.receive(old) == 0 && old.buffer.pool == generation && old.buffer.fd == 100 && seek->capture_queued - queued_before == int(seek->capture_granted),
                    "the flushed pool keeps delivering frames, and the retired frame's slot is requeued on the decode thread");
            seek->calls.clear();
            require(decoder.submit(packet, 1234567) == 0 && seek->output_on, "the coded queue restarts on the next access unit, with a buffer queued");
            old = {};
            // The sought keyframe's headers announce a resolution change, and that
            // still renegotiates: the capture pool only, never the coded queue,
            // whose buffers the driver kept.
            seek->capture_pitch = 128;
            seek->capture_bytes = 12288;
            require(decoder.receive(empty) == -EAGAIN && seek->closes == 8 && seek->exports == exports_before + 8 && seek->capture_on,
                    "a source change after a seek replaces the capture pool without an allocation for the coded queue");
            seek->produce = true;
            require(decoder.receive(old) == 0 && old.buffer.pool != generation && old.buffer.pitches[0] == 128 && old.buffer.offsets[1] == 8192 && old.buffer.size == 12288,
                    "the frames after the change come from the freshly negotiated pool");
            old = {};
        }
        for(int fault = 0; fault < 4; ++fault) {
            ScopedEnvironment restart("LUMAFRAMEVR_V4L2_SEEK_RESTART", "1");  // the faults are on that path
            auto bad = std::make_shared<QueueDevice>();
            {
                media::V4l2Decoder decoder(bad, stream);
                decoder.submit(packet, 1234567);
                media::V4l2Frame empty;
                decoder.receive(empty);
                if(fault == 0)
                    bad->fail_request = VIDIOC_STREAMOFF;
                if(fault == 1)
                    bad->fail_request = VIDIOC_QBUF;  // the requeue of an unleased slot
                if(fault == 2)
                    bad->fail_request = VIDIOC_STREAMON;
                if(fault == 3)
                    bad->poll_error = EIO;
                rejects([&] { decoder.reset(); }, "seek flush failures propagate to automatic fallback");
                require(bad->exports == 8 && bad->maps == 2 && bad->capture_proposals == 1, "a failed flush never allocates, exports or renegotiates anything");
                require(decoder.submit(packet, 1234567) == -EAGAIN && decoder.drain() == -EAGAIN, "partially flushed queue never resumes decoding");
            }
            require(bad->maps == bad->unmaps && bad->closes == bad->exports, "failed seek releases mapped buffers and exports exactly once");
        }
        auto changing = std::make_shared<QueueDevice>();
        {
            media::V4l2Decoder decoder(changing, stream);
            decoder.submit(packet, 1234567);
            media::V4l2Frame old, empty;
            decoder.receive(empty);
            changing->source_change_event = true;
            changing->produce = changing->last = true;
            require(decoder.receive(old) == 0, "source change drains old pool through LAST payload");
            const auto generation = old.buffer.pool;
            require(decoder.submit(packet, 1234567) == -EAGAIN, "new input pauses during capture replacement");
            require(decoder.drain() == -EAGAIN && !changing->stopped, "EOS waits for capture replacement");
            require(decoder.receive(empty) == -EAGAIN && changing->closes == 0 && !changing->capture_on, "capture stops before waiting for graphics release");
            const auto stopped_calls = changing->calls.size();
            require(decoder.receive(empty) == -EAGAIN && changing->calls.size() > stopped_calls && changing->capture_queued == 8 && changing->capture_proposals == 1,
                    "stopped capture is neither requeued nor renegotiated while leased");
            ScopedEnvironment restart("LUMAFRAMEVR_V4L2_SEEK_RESTART", "1");
            // The same flush a seek performs, taken while a capture change is
            // pending: the sequence that would have ended with a LAST buffer was
            // discarded here, so this change is served without waiting for one.
            require(decoder.reset() == 0 && changing->capture_on && !changing->output_on && changing->capture_queued == 15,
                    "a flush during a pending change requeues the unleased slots and restarts capture");
            old = {};
            changing->last = false;
            require(decoder.receive(empty) == -EAGAIN && changing->closes == 8 && changing->exports == 16, "released old pool is replaced without ending decode");
            changing->produce = true;
            require(decoder.receive(old) == 0 && old.buffer.pool != generation, "replacement pool has distinct import cache identity");
            old = {};
        }
        require(changing->exports == changing->closes, "both generations close exports");
        // A change the driver announced and has not yet ended with a LAST buffer -
        // the event is in, the old sequence's pictures are still arriving - and then
        // a seek. On the stop-and-restart path the flush discards the buffer that
        // would have ended that sequence, so the change must be served without one;
        // waiting for a LAST that can never arrive would stall the queue. That path
        // is the comparison knob's, and so is this case.
        {
            ScopedEnvironment restart("LUMAFRAMEVR_V4L2_SEEK_RESTART", "1");
            auto pending = std::make_shared<QueueDevice>();
            media::V4l2Decoder decoder(pending, stream);
            decoder.submit(packet, 1234567);
            media::V4l2Frame old, empty;
            decoder.receive(empty);
            pending->source_change_event = true;
            pending->produce = true;  // an old-sequence picture, not its LAST
            require(decoder.receive(old) == 0, "an announced change delivers the old sequence's pictures");
            const auto generation = old.buffer.pool;
            require(decoder.submit(packet, 1234567) == -EAGAIN, "a change in flight stops new input");
            require(decoder.reset() == 0 && pending->capture_on && !pending->output_on && pending->capture_queued == 15, "a seek flushes the pool a change is draining and restarts its capture queue");
            old = {};
            require(decoder.receive(empty) == -EAGAIN && pending->closes == 8 && pending->exports == 16, "the flushed change is served without the LAST buffer the flush discarded");
            pending->produce = true;
            require(decoder.receive(old) == 0 && old.buffer.pool != generation, "the replaced pool carries the new sequence");
            old = {};
        }
        for(unsigned long request :
            {VIDIOC_QUERYCAP, VIDIOC_SUBSCRIBE_EVENT, VIDIOC_S_FMT, VIDIOC_REQBUFS, VIDIOC_QUERYBUF, VIDIOC_EXPBUF, VIDIOC_G_FMT, VIDIOC_DQBUF, VIDIOC_G_SELECTION, VIDIOC_STREAMON}) {
            auto bad = std::make_shared<QueueDevice>();
            bad->fail_request = request;
            rejects(
                    [&] {
                        media::V4l2Decoder decoder(bad, stream);
                        decoder.submit(packet, 1234567);
                        media::V4l2Frame frame;
                        decoder.receive(frame);
                    },
                    "ioctl failure propagates");
            require(bad->closes == bad->exports && bad->unmaps == bad->maps, "ioctl failure cleanup is balanced");
        }
        for(bool poll_failure : {false, true}) {
            auto bad = std::make_shared<QueueDevice>();
            media::V4l2Decoder decoder(bad, stream);
            require(decoder.submit(packet, 1234567) == 0, "prime real event error fixture");
            if(poll_failure)
                bad->poll_error = EIO;
            else
                bad->fail_request = VIDIOC_DQEVENT;
            media::V4l2Frame frame;
            rejects([&] { decoder.receive(frame); }, "real poll and event I/O errors remain fatal");
        }
        auto deferred_stop = std::make_shared<QueueDevice>();
        {
            media::V4l2Decoder decoder(deferred_stop, stream);
            decoder.submit(packet, 1234567);
            media::V4l2Frame frame;
            decoder.receive(frame);
            deferred_stop->stop_error = EAGAIN;
            require(decoder.drain() == -EAGAIN && !deferred_stop->stopped, "decoder STOP backpressure is retryable and does not mark draining");
            deferred_stop->stop_error = 0;
            require(decoder.drain() == 0 && deferred_stop->stopped, "decoder STOP succeeds on retry");
            require(decoder.drain() == 0, "drain is idempotent after STOP");
            require(std::count(deferred_stop->calls.begin(), deferred_stop->calls.end(), VIDIOC_DECODER_CMD) == 2, "successful STOP is not repeated");
        }
        auto off = std::make_shared<QueueDevice>();
        {
            media::V4l2Decoder decoder(off, stream);
            decoder.submit(packet, 1234567);
            media::V4l2Frame frame;
            decoder.receive(frame);
            off->fail_request = VIDIOC_STREAMOFF;
        }
        require(off->closes == off->exports && off->maps == off->unmaps && std::count(off->calls.begin(), off->calls.end(), VIDIOC_STREAMOFF) == 2,
                "STREAMOFF failure during cleanup does not abort resource release");
        auto multiple_planes = std::make_shared<QueueDevice>();
        multiple_planes->capture_planes = 2;
        rejects(
                [&] {
                    media::V4l2Decoder decoder(multiple_planes, stream);
                    decoder.submit(packet, 1234567);
                    media::V4l2Frame frame;
                    decoder.receive(frame);
                },
                "two memory planes are not mislabelled as one contiguous export");
        require(multiple_planes->exports == 0, "unsupported memory-plane topology rejected before export");
        auto bounds = std::make_shared<QueueDevice>();
        {
            media::V4l2Decoder decoder(bounds, stream);
            rejects([&] { decoder.submit({}, 0); }, "empty access unit is not EOS");
            rejects([&] { decoder.submit(std::vector<uint8_t>(4097), 0); }, "oversized access unit is not truncated");
            require(bounds->output_queued == 0, "invalid input never enters output queue");
            require(decoder.drain() == 0, "empty stream drains without ioctl STOP");
            media::V4l2Frame frame;
            require(decoder.receive(frame) == AVERROR_EOF && !bounds->stopped, "empty stream EOF");
        }
        auto corrupt = std::make_shared<QueueDevice>();
        media::V4l2Decoder decoder(corrupt, stream);
        decoder.submit(packet, 1234567);
        media::V4l2Frame frame;
        decoder.receive(frame);
        corrupt->produce = corrupt->error_frame = true;
        rejects([&] { decoder.receive(frame); }, "driver decode error not delivered as valid picture");
    });
}
