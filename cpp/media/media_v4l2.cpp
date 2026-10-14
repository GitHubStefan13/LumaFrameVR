#include "media_v4l2.hpp"
#include "v4l2_decoder.hpp"
#include "media_result.hpp"
#include "media_decoder_startup.hpp"
#include "media_codec.hpp"
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <vector>
extern "C" {
#include <libavcodec/bsf.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/time.h>
}

namespace {
// AVFrame copies opaque on clone; the static tag outlives every capture lease.
char source_layout_tag;
void checked(int result, const char* operation) {
    if(result >= 0)
        return;
    char detail[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(result, detail, sizeof(detail));
    throw std::runtime_error(std::string(operation) + ": " + detail);
}
// The single description of a stream for the V4L2 queue, shared by the decoder policy and the
// playback adapter so they can never disagree about what the device will accept.
media::V4l2Stream stream_spec(const AVCodecParameters& p) {
    const auto* pixel = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(p.format));
    return media::V4l2Stream{p.codec_id == AV_CODEC_ID_H264   ? media::V4l2Codec::H264
                             : p.codec_id == AV_CODEC_ID_HEVC ? media::V4l2Codec::Hevc
                             : p.codec_id == AV_CODEC_ID_VP9  ? media::V4l2Codec::Vp9
                                                              : media::V4l2Codec::Unsupported,
                             static_cast<uint32_t>(p.width), static_cast<uint32_t>(p.height), pixel ? static_cast<uint32_t>(pixel->comp[0].depth) : 0,
                             pixel && pixel->log2_chroma_w == 1 && pixel->log2_chroma_h == 1};
}
std::unique_ptr<media::V4l2Decoder> device(media::V4l2Stream stream) {
    std::vector<std::string> paths;
    if(const char* requested = std::getenv("LUMAFRAMEVR_V4L2_DEVICE"))
        paths.emplace_back(requested);
    else
        paths = media::v4l2_device_nodes();
    if(paths.empty())
        throw std::runtime_error("no V4L2 decoder device (/dev/video*)");
    std::string reasons;
    for(const auto& path : paths) {
        try {
            auto decoder = media::V4l2Decoder::open(path, stream);
            std::fprintf(stderr, "Decoder: own V4L2 queue on %s\n", path.c_str());
            return decoder;
        } catch(const std::exception& error) {
            if(!reasons.empty())
                reasons += "; ";
            reasons += path + ": " + error.what();
        }
    }
    // A refusal to allocate is the one failure whose cause is usually outside this
    // process: the VPU's memory is device wide, and Steam's own web helper opens
    // one of these nodes. Name whoever holds them, so the log says who rather than
    // leaving the reader to guess between our bug and someone else's client.
    if(reasons.find("Cannot allocate memory") != std::string::npos && reasons.find("the VPU is held by") == std::string::npos) {
        const std::string holders = media::vpu_holder_report(paths);
        if(!holders.empty())
            reasons += "; the VPU is held by " + holders;
    }
    throw std::runtime_error(reasons);
}
struct ExportedFrame {
    // First member is the existing FFmpeg DRM descriptor, not a pixel copy.
    AVDRMFrameDescriptor descriptor{};
    std::shared_ptr<void> lease;
};
void release_export(void* opaque, uint8_t*) {
    delete static_cast<ExportedFrame*>(opaque);
}
}  // namespace

bool media::v4l2_accepts(const AVCodecParameters& parameters) {
    return v4l2_formats(stream_spec(parameters)).has_value();
}

bool media::v4l2_source_layout(const AVFrame& frame) {
    return frame.opaque == &source_layout_tag;
}

void media::export_v4l2_frame(Media& media, V4l2Frame decoded) {
    auto exported = std::make_unique<ExportedFrame>();
    const auto& d = decoded.buffer;
    auto& drm = exported->descriptor;
    drm.nb_objects = drm.nb_layers = 1;
    drm.objects[0].fd = d.fd;
    drm.objects[0].size = d.size;
    drm.objects[0].format_modifier = d.modifier;
    drm.layers[0].format = d.format;
    drm.layers[0].nb_planes = 2;
    for(int i = 0; i < 2; ++i) {
        drm.layers[0].planes[i].object_index = 0;
        drm.layers[0].planes[i].offset = d.offsets[i];
        drm.layers[0].planes[i].pitch = d.pitches[i];
    }
    exported->lease = std::move(decoded.owner);
    auto* frame = media.frame;
    frame->buf[0] = av_buffer_create(reinterpret_cast<uint8_t*>(&drm), sizeof(drm), release_export, exported.get(), AV_BUFFER_FLAG_READONLY);
    if(!frame->buf[0])
        throw std::bad_alloc();
    exported.release();
    frame->data[0] = frame->buf[0]->data;
    frame->format = AV_PIX_FMT_DRM_PRIME;
    frame->opaque = &source_layout_tag;
    frame->width = d.width;
    frame->height = d.height;
    frame->crop_left = d.crop[0];
    frame->crop_top = d.crop[1];
    frame->crop_right = d.crop[2];
    frame->crop_bottom = d.crop[3];
    auto* stream = media.file->streams[media.stream];
    auto* p = stream->codecpar;
    frame->best_effort_timestamp = frame->pts = av_rescale_q(decoded.timestamp_us, AV_TIME_BASE_Q, stream->time_base);
    frame->colorspace = p->color_space;
    frame->color_range = p->color_range;
    frame->color_trc = p->color_trc;
    frame->color_primaries = p->color_primaries;
    frame->chroma_location = p->chroma_location;
    frame->sample_aspect_ratio = p->sample_aspect_ratio;
    // Preserve container HDR side data, which would otherwise be delivered by avcodec.
    for(int i = 0; i < p->nb_coded_side_data; ++i) {
        const auto& side = p->coded_side_data[i];
        if(side.type != AV_PKT_DATA_MASTERING_DISPLAY_METADATA && side.type != AV_PKT_DATA_CONTENT_LIGHT_LEVEL)
            continue;
        const auto type = side.type == AV_PKT_DATA_MASTERING_DISPLAY_METADATA ? AV_FRAME_DATA_MASTERING_DISPLAY_METADATA : AV_FRAME_DATA_CONTENT_LIGHT_LEVEL;
        auto* copy = av_frame_new_side_data(frame, type, side.size);
        if(!copy)
            throw std::bad_alloc();
        std::copy_n(side.data, side.size, copy->data);
    }
    // Keep the queue's actual pool generation (including SOURCE_CHANGE) intact.
    media.decoder_generation = d.pool;
}
namespace {
int64_t stall_budget() {
    const char* value = std::getenv("LUMAFRAMEVR_DECODE_STALL_MS");
    const long ms = value ? std::strtol(value, nullptr, 10) : 0;
    return ms > 0 && ms <= INT64_MAX / 1000 ? ms * 1000 : 4000000;
}
}  // namespace

struct media::V4l2Playback::State {
    std::unique_ptr<V4l2Decoder> decoder;
    Opener opener;
    bool retried = false, frame_delivered = false;
    AVBSFContext* filter = nullptr;
    AVCodecParserContext* parser = nullptr;
    AVPacket* filtered = av_packet_alloc();
    uint8_t* access_unit = nullptr;
    int access_bytes = 0;
    int64_t timestamp_us = 0;
    bool filter_draining = false, drained = false;
    ~State() {
        av_packet_free(&filtered);
        av_bsf_free(&filter);
        av_parser_close(parser);
    }
};
media::V4l2Playback::~V4l2Playback() = default;
media::V4l2Playback::V4l2Playback(Media& media, Opener opener) : state_(std::make_unique<State>()) {
    auto* stream = media.file->streams[media.stream];
    auto* p = stream->codecpar;
    const V4l2Stream spec = stream_spec(*p);
    if(!v4l2_formats(spec))
        throw std::runtime_error("unsupported V4L2 codec, depth or chroma layout");
    auto& s = *state_;
    s.opener = opener ? std::move(opener) : Opener(device);
    s.decoder = s.opener(spec);
    if(!s.filtered)
        throw std::bad_alloc();
    const char* name = spec.codec == V4l2Codec::H264 ? "h264_mp4toannexb" : spec.codec == V4l2Codec::Hevc ? "hevc_mp4toannexb" : "vp9_superframe_split";
    const auto* filter = av_bsf_get_by_name(name);
    if(!filter)
        throw std::runtime_error(std::string("FFmpeg bitstream filter unavailable: ") + name);
    checked(av_bsf_alloc(filter, &s.filter), "allocate FFmpeg bitstream filter");
    checked(avcodec_parameters_copy(s.filter->par_in, p), "copy stream parameters");
    s.filter->time_base_in = stream->time_base;
    checked(av_bsf_init(s.filter), "initialize FFmpeg bitstream filter");
    // Allocate only a description/parser context. No FFmpeg decoder is opened on this path.
    avcodec_free_context(&media.decoder);
    media.decoder = avcodec_alloc_context3(software_decoder(p));
    if(!media.decoder)
        throw std::bad_alloc();
    checked(avcodec_parameters_to_context(media.decoder, s.filter->par_out), "describe V4L2 stream");
    media.decoder->pkt_timebase = stream->time_base;
    s.parser = av_parser_init(p->codec_id);
    if(!s.parser)
        throw std::runtime_error("FFmpeg access-unit parser unavailable");
    // libavformat packets and the filters above are complete access units.
    s.parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;
}

void media::V4l2Playback::reset(Media& media) {
    auto& s = *state_;
    // A seek is a flush: the driver keeps both allocations, so no allocation can
    // be refused, and a frame still leased from before the seek costs nothing -
    // its slot simply goes back when the consumer retires it. The one thing that
    // still has to wait for old frames is a capture pool being replaced, and that
    // wait lives with replace_capture(), which answers EAGAIN until the frames
    // holding the pool are gone. Kept here as the same bounded guard: never treat
    // "not yet" as a reason to write the VPU off for the rest of the file.
    for(int attempt = 0;; ++attempt) {
        const int result = s.decoder->reset();
        if(result != -EAGAIN) {
            checked(result, "reset V4L2 queues");
            break;
        }
        if(attempt == 4) {
            std::fprintf(stderr, "Decoder: V4L2 queues were still leased by %llu frame(s) across 5 seek attempts over %llu ms; giving up on the VPU for this file\n",
                         static_cast<unsigned long long>(media::outstanding_capture_leases()), static_cast<unsigned long long>(attempt * 250));
            checked(result, "reset V4L2 queues");
        }
        media::await_capture_leases(false);  // the frames only: this decoder's own queue is alive
    }
    av_bsf_flush(s.filter);
    av_packet_unref(s.filtered);
    av_parser_close(s.parser);
    s.parser = av_parser_init(media.file->streams[media.stream]->codecpar->codec_id);
    if(!s.parser)
        throw std::runtime_error("FFmpeg access-unit parser unavailable after seek");
    s.parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;
    s.access_unit = nullptr;
    s.access_bytes = 0;
    s.filter_draining = s.drained = false;
    media.first_frame_observed = media.flushing_at_end = 0;
    media.stall_started_us = 0;
}

int media::reset_v4l2_decoder(Media& media) {
    try {
        media.v4l2->reset(media);
        return 0;
    } catch(const std::exception& error) {
        return fallback_v4l2(media, error.what(), false);
    }
}

media::Result media::V4l2Playback::receive(Media& media) {
    try {
        return receive_once(media);
    } catch(const std::system_error& error) {
        auto& s = *state_;
        // An intermittent initial VPU failure gets exactly one fresh session.
        // Never retry corrupt streams forever or hide errors after playback.
        const int code = error.code().value();
        if(s.retried || s.frame_delivered || media.interrupted.load() || (code != EIO && code != EBUSY))
            throw;
        s.retried = true;
        std::fprintf(stderr, "Decoder: own V4L2 startup failed (%s); retrying once after 50 ms\n", error.what());
        auto opener = s.opener;
        s.decoder.reset();  // close old queues/device before waiting or reopening
        av_frame_unref(media.frame);
        // A retained frame keeps the old queue's buffers allocated, and the
        // replacement cannot request its own while they are held: REQBUFS
        // answers ENOMEM. Retire those leases before the rewind and the rebuild;
        // the wait is bounded and reports what is still held when it expires.
        media::await_capture_leases();
        const double resume = std::max(0.0, media.fallback_pts);
        auto* stream = media.file->streams[media.stream];
        const int64_t offset = stream->start_time == AV_NOPTS_VALUE ? 0 : stream->start_time;
        const int64_t target = av_rescale_q(static_cast<int64_t>(resume * AV_TIME_BASE), AV_TIME_BASE_Q, stream->time_base) + offset;
        media_reader_pause(&media, 1);
        av_packet_unref(media.packet);
        const int sought = media_seek_file(&media, target);
        const int audio = audio_reset(media.audio, resume);
        media_reader_pause(&media, 0);
        checked(sought, "rewind V4L2 startup retry");
        if(audio < 0)
            std::fprintf(stderr, "Audio: startup retry reset failed (%d); continuing playback\n", audio);
        av_usleep(50000);
        if(media.interrupted.load())
            return {0, {}};
        V4l2Playback replacement(media, std::move(opener));
        state_ = std::move(replacement.state_);
        state_->retried = true;
        media.flushing_at_end = 0;
        media.stall_started_us = 0;
        media.seek_target = media.fallback_pts = resume;
        media.preview_repeat_requested = 0;
    }
    // A failed second attempt escapes to the existing automatic FFmpeg fallback.
    return receive_once(media);
}

media::Result media::V4l2Playback::receive_once(Media& media) {
    auto& s = *state_;
    av_frame_unref(media.frame);
    for(;;) {
        if(media.interrupted.load())
            return {0, {}};
        V4l2Frame frame;
        const int received = s.decoder->receive(frame);
        if(received == 0) {
            export_v4l2_frame(media, std::move(frame));
            s.frame_delivered = true;
            media.stall_started_us = 0;
            return {1, {}};
        }
        if(received == AVERROR_EOF)
            return {0, {}};
        checked(received == -EAGAIN ? 0 : received, "V4L2 receive");
        if(!media.stall_started_us)
            media.stall_started_us = av_gettime_relative();
        if(av_gettime_relative() - media.stall_started_us > stall_budget())
            throw std::runtime_error("V4L2 queue stalled without a frame (including drain)");
        if(s.access_bytes) {
            const int sent = s.decoder->submit({s.access_unit, static_cast<size_t>(s.access_bytes)}, s.timestamp_us);
            if(sent == -EAGAIN)
                return {2, {}};
            checked(sent, "V4L2 submit");
            s.access_bytes = 0;
            av_packet_unref(s.filtered);
            continue;
        }
        if(s.drained)
            return {2, {}};
        int result = av_bsf_receive_packet(s.filter, s.filtered);
        if(result == 0) {
            if(s.filtered->pts == AV_NOPTS_VALUE)
                throw std::runtime_error("V4L2 access unit has no presentation timestamp");
            const int consumed = av_parser_parse2(s.parser, media.decoder, &s.access_unit, &s.access_bytes, s.filtered->data, s.filtered->size, s.filtered->pts, s.filtered->dts, s.filtered->pos);
            checked(consumed, "FFmpeg parse V4L2 access unit");
            if(consumed != s.filtered->size || !s.access_bytes)
                throw std::runtime_error("FFmpeg parser did not produce a complete V4L2 access unit");
            if(s.parser->pts == AV_NOPTS_VALUE)
                throw std::runtime_error("V4L2 parsed access unit has no presentation timestamp");
            s.timestamp_us = av_rescale_q(s.parser->pts, s.filter->time_base_out, AV_TIME_BASE_Q);
            continue;
        }
        if(result == AVERROR_EOF) {
            const int drained = s.decoder->drain();
            if(drained == -EAGAIN)
                return {2, {}};
            checked(drained, "V4L2 drain");
            s.drained = true;
            media.flushing_at_end = 1;
            return {2, {}};
        }
        checked(result == AVERROR(EAGAIN) ? 0 : result, "FFmpeg receive bitstream");
        if(s.filter_draining)
            throw std::runtime_error("FFmpeg bitstream filter stalled during drain");
        result = media.packet->size ? 0 : media_read_packet(&media);
        if(result == AVERROR(EAGAIN) || result == AVERROR(ENOBUFS))
            return {2, {}};
        if(result == AVERROR_EOF) {
            checked(av_bsf_send_packet(s.filter, nullptr), "drain FFmpeg bitstream filter");
            s.filter_draining = true;
        } else {
            checked(result, "read V4L2 demux packet");
            checked(av_bsf_send_packet(s.filter, media.packet), "convert V4L2 bitstream");
        }
    }
}

int media::fallback_v4l2(Media& media, std::string_view reason, bool rewind) {
    std::fprintf(stderr, "Decoder: own V4L2 unavailable (%.*s); falling back to FFmpeg software\n", static_cast<int>(reason.size()), reason.data());
    media.v4l2.reset();
    media.skip_hardware_decoder = 1;  // Per-file only; do not demote subsequent files.
    av_frame_unref(media.frame);
    const int result = open_software_decoder(media);
    if(result < 0)
        return result;
    media.stall_started_us = 0;
    if(rewind) {
        // Replay from the previous keyframe so reference frames already consumed by
        // V4L2 are available to FFmpeg; suppress pictures before the playback position.
        const double resume = std::max(0.0, media.fallback_pts);
        auto* stream = media.file->streams[media.stream];
        const int64_t offset = stream->start_time == AV_NOPTS_VALUE ? 0 : stream->start_time;
        const int64_t target = av_rescale_q(static_cast<int64_t>(resume * AV_TIME_BASE), AV_TIME_BASE_Q, stream->time_base) + offset;
        media_reader_pause(&media, 1);
        av_packet_unref(media.packet);
        const int sought = media_seek_file(&media, target);
        const int audio = audio_reset(media.audio, resume);
        media_reader_pause(&media, 0);
        if(sought < 0) {
            char detail[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(sought, detail, sizeof(detail));
            std::fprintf(stderr, "Decoder: FFmpeg fallback cannot rewind (%s); continuing at the next demux access unit\n", detail);
        }
        if(audio < 0)
            std::fprintf(stderr, "Audio: fallback reset failed (%d); continuing playback\n", audio);
        media.seek_target = media.fallback_pts = resume;
        media.preview_repeat_requested = 0;
    }
    return 0;
}
int media::open_v4l2_decoder(Media& media) {
    media.v4l2.reset();
    try {
        media.v4l2 = std::make_unique<V4l2Playback>(media);
        media.software_decoding = media.first_frame_observed = media.flushing_at_end = 0;
        media.stall_started_us = 0;
        ++media.decoder_generation;
        media.info = {};
        return 0;
    } catch(const std::exception& error) {
        return fallback_v4l2(media, error.what(), false);
    }
}
