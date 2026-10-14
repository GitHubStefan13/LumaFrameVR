#include "test_registry.hpp"
#include "test_assertions.hpp"
#include "v4l2_decoder.hpp"
#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

namespace {
struct Fixture {
    AVFormatContext* file = nullptr;
    AVCodecContext* software = nullptr;
    AVBSFContext* filter = nullptr;
    AVCodecContext* parser_description = nullptr;
    AVCodecParserContext* parser = nullptr;
    AVPacket* input = av_packet_alloc();
    AVPacket* filtered = av_packet_alloc();
    AVFrame* decoded = av_frame_alloc();
    std::vector<AVFrame*> reference;
    struct Packet { std::vector<uint8_t> bytes; int64_t pts; };
    std::vector<Packet> packets;
    AVRational time_base{};
    media::V4l2Stream stream{};
    ~Fixture() {
        for(auto* frame : reference) av_frame_free(&frame);
        av_frame_free(&decoded);
        av_packet_free(&filtered); av_packet_free(&input);
        av_bsf_free(&filter); avcodec_free_context(&software);
        av_parser_close(parser); avcodec_free_context(&parser_description);
        avformat_close_input(&file);
    }
    void collect_reference() {
        int result;
        while((result = avcodec_receive_frame(software, decoded)) == 0) {
            require(reference.size() < 120, "device comparison fixture limited to 120 frames");
            require(decoded->width <= 1024 && decoded->height <= 1024, "use a small diagnostic fixture, not 8K content");
            auto* copy = av_frame_clone(decoded);
            require(copy != nullptr, "clone software reference");
            reference.push_back(copy); av_frame_unref(decoded);
        }
        require(result == AVERROR(EAGAIN) || result == AVERROR_EOF, "software fixture decodes cleanly");
    }
    void collect_packets() {
        if(!filter) {
            packets.push_back({{input->data, input->data + input->size}, av_rescale_q(input->pts, time_base, AVRational{1, 1000000})});
            return;
        }
        require(av_bsf_send_packet(filter, input) == 0, "submit demux packet to Annex-B converter");
        int result;
        while((result = av_bsf_receive_packet(filter, filtered)) == 0) {
            require(filtered->pts != AV_NOPTS_VALUE, "fixture access unit has timestamp");
            uint8_t* access_unit = nullptr;
            int access_bytes = 0;
            const int consumed = av_parser_parse2(parser, parser_description, &access_unit, &access_bytes,
                    filtered->data, filtered->size, filtered->pts, filtered->dts, filtered->pos);
            require(consumed == filtered->size && access_bytes > 0 && parser->pts == filtered->pts,
                    "real FFmpeg complete-AU parser preserves bitstream presentation PTS");
            packets.push_back({{access_unit, access_unit + access_bytes}, av_rescale_q(parser->pts, filter->time_base_out, AVRational{1, 1000000})});
            av_packet_unref(filtered);
        }
        require(result == AVERROR(EAGAIN) || result == AVERROR_EOF, "bitstream conversion succeeds");
    }
    void load(const char* path) {
        require(input && filtered && decoded, "allocate comparison fixture");
        require(avformat_open_input(&file, path, nullptr, nullptr) == 0 && avformat_find_stream_info(file, nullptr) >= 0, "open comparison fixture");
        const int index = av_find_best_stream(file, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        require(index >= 0, "fixture has video stream");
        auto* video = file->streams[index]; auto* p = video->codecpar;
        time_base = video->time_base;
        auto* pixel = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(p->format));
        require(pixel && pixel->log2_chroma_w == 1 && pixel->log2_chroma_h == 1, "fixture has 420 pixels");
        stream = {p->codec_id == AV_CODEC_ID_H264 ? media::V4l2Codec::H264 : p->codec_id == AV_CODEC_ID_HEVC ? media::V4l2Codec::Hevc :
                  p->codec_id == AV_CODEC_ID_VP9 ? media::V4l2Codec::Vp9 : media::V4l2Codec::Unsupported,
                  static_cast<uint32_t>(p->width), static_cast<uint32_t>(p->height), static_cast<uint32_t>(pixel->comp[0].depth)};
        require(media::v4l2_formats(stream).has_value(), "fixture has supported V4L2 stream");
        software = avcodec_alloc_context3(avcodec_find_decoder(p->codec_id));
        require(software && avcodec_parameters_to_context(software, p) == 0, "configure software reference");
        software->pkt_timebase = time_base;
        require(avcodec_open2(software, software->codec, nullptr) == 0, "open software reference");
        const char* bsf = stream.codec == media::V4l2Codec::H264 ? "h264_mp4toannexb" :
                          stream.codec == media::V4l2Codec::Hevc ? "hevc_mp4toannexb" : "vp9_superframe_split";
        require(av_bsf_alloc(av_bsf_get_by_name(bsf), &filter) == 0 && filter, "allocate FFmpeg bitstream converter");
        require(avcodec_parameters_copy(filter->par_in, p) == 0, "copy bitstream parameters");
        filter->time_base_in = time_base;
        require(av_bsf_init(filter) == 0, "initialize FFmpeg bitstream conversion");
        parser_description = avcodec_alloc_context3(avcodec_find_decoder(p->codec_id));
        require(parser_description && avcodec_parameters_to_context(parser_description, filter->par_out) == 0,
                "describe access units without opening an FFmpeg decoder");
        parser_description->pkt_timebase = time_base;
        parser = av_parser_init(p->codec_id);
        require(parser != nullptr, "allocate real FFmpeg access-unit parser");
        parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;
        int result;
        while((result = av_read_frame(file, input)) == 0) {
            if(input->stream_index == index) {
                require(input->pts != AV_NOPTS_VALUE, "fixture packet has timestamp");
                require(avcodec_send_packet(software, input) == 0, "feed software reference");
                collect_reference(); collect_packets();
            }
            av_packet_unref(input);
        }
        require(result == AVERROR_EOF, "fixture demux reaches EOF");
        require(avcodec_send_packet(software, nullptr) == 0, "drain software reference");
        collect_reference();
        require(av_bsf_send_packet(filter, nullptr) == 0 && av_bsf_receive_packet(filter, filtered) == AVERROR_EOF,
                "Annex-B converter has no unsubmitted access units at EOF");
        require(!reference.empty() && !packets.empty(), "comparison fixture is not vacuous");
    }
};
struct MappedCapture {
    int fd;
    void* data;
    uint64_t size;
    explicit MappedCapture(const media::DmaBuf& buffer) : fd(buffer.fd), size(buffer.size) {
        require(buffer.modifier == 0, "only compare linear capture memory");
        dma_buf_sync sync{}; sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ;
        require(::ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync) == 0, "synchronize DMA-BUF CPU read");
        data = ::mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
        if(data == MAP_FAILED) {
            sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ; ::ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
            throw std::runtime_error("map capture for pixel comparison");
        }
    }
    ~MappedCapture() {
        ::munmap(data, size);
        dma_buf_sync sync{}; sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ;
        ::ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
    }
};
void compare(const media::V4l2Frame& actual, const AVFrame& expected, AVRational time_base) {
    const auto& d = actual.buffer;
    require(d.width - d.crop[0] - d.crop[2] == expected.width && d.height - d.crop[1] - d.crop[3] == expected.height, "visible capture matches software geometry");
    require(d.crop[0] % 2 == 0 && d.crop[1] % 2 == 0, "comparison crop is chroma aligned");
    const bool ten = d.format == media::v4l2_p010;
    require(expected.format == (ten ? AV_PIX_FMT_YUV420P10LE : AV_PIX_FMT_YUV420P), "reference is planar 420 at capture depth");
    require(actual.timestamp_us == av_rescale_q(expected.best_effort_timestamp, time_base, AVRational{1, 1000000}), "frame-by-frame timestamps match");
    MappedCapture mapped(d);
    const auto* bytes = static_cast<const uint8_t*>(mapped.data);
    for(int plane = 0; plane < 3; ++plane) {
        const int width = plane ? (expected.width + 1) / 2 : expected.width;
        const int height = plane ? (expected.height + 1) / 2 : expected.height;
        for(int y = 0; y < height; ++y) {
            const int crop_y = plane ? d.crop[1] / 2 : d.crop[1];
            const int crop_x = plane ? d.crop[0] / 2 : d.crop[0];
            const auto* source = bytes + d.offsets[plane ? 1 : 0] + (y + crop_y) * d.pitches[plane ? 1 : 0];
            const auto* target = expected.data[plane] + y * expected.linesize[plane];
            for(int x = 0; x < width; ++x) {
                const int sample = plane ? (x + crop_x) * 2 + plane - 1 : x + crop_x;
                if(ten) {
                    uint16_t a, b; std::memcpy(&a, source + sample * 2, 2); std::memcpy(&b, target + x * 2, 2);
                    require((a >> 6) == b, "P010 sample equals FFmpeg 10-bit sample");
                } else require(source[sample] == target[x], "NV12 sample equals FFmpeg 8-bit sample");
            }
        }
    }
}
}  // namespace

int lumaframevr::tests::v4l2_bitstream(int, char**) {
    return acceptance_main([] {
        const char* path = std::getenv("LUMAFRAMEVR_TEST_VIDEO");
        if(!path) skip("LUMAFRAMEVR_TEST_VIDEO is not set");
        Fixture fixture; fixture.load(path);
        if(fixture.stream.codec == media::V4l2Codec::H264 || fixture.stream.codec == media::V4l2Codec::Hevc) {
            for(const auto& packet : fixture.packets) {
                require(packet.bytes.size() >= 4 && packet.bytes[0] == 0 && packet.bytes[1] == 0 &&
                        (packet.bytes[2] == 1 || (packet.bytes[2] == 0 && packet.bytes[3] == 1)), "converted access unit starts with Annex-B start code");
            }
        }
        require(fixture.packets.size() >= fixture.reference.size(), "converter retains all decodable access units");
    });
}

int lumaframevr::tests::v4l2_device(int, char**) {
    return acceptance_main([] {
        const char* requested = std::getenv("LUMAFRAMEVR_TEST_V4L2_DEVICE");
        const std::string path = requested ? requested : "/dev/video-dec0";
        if(!std::filesystem::exists(path)) skip("no V4L2 decoder device; real ioctl queue and pixel comparison unverified");
        const char* video = std::getenv("LUMAFRAMEVR_TEST_VIDEO");
        if(!video) skip("LUMAFRAMEVR_TEST_VIDEO is not set");
        Fixture fixture; fixture.load(video);
        auto decoder = media::V4l2Decoder::open(path, fixture.stream);
        size_t sent = 0, received = 0;
        bool draining = false, eof = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while(!eof && std::chrono::steady_clock::now() < deadline) {
            media::V4l2Frame frame;
            const int result = decoder->receive(frame);
            if(result == 0) {
                require(received < fixture.reference.size(), "no extra V4L2 frames");
                compare(frame, *fixture.reference[received++], fixture.time_base);
            } else if(result == AVERROR_EOF) eof = true;
            else require(result == -EAGAIN, "V4L2 capture reports only frame, backpressure or EOF");
            if(sent < fixture.packets.size()) {
                const auto& packet = fixture.packets[sent];
                const int submitted = decoder->submit(packet.bytes, packet.pts);
                require(submitted == 0 || submitted == -EAGAIN, "V4L2 fixture submit");
                if(submitted == 0) ++sent;
            } else if(!draining) {
                const int drained = decoder->drain();
                require(drained == 0 || drained == -EAGAIN, "V4L2 fixture drain");
                draining = drained == 0;
            }
            if(result == -EAGAIN) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        require(eof && received == fixture.reference.size(), "full V4L2 frame sequence equals FFmpeg through drained EOF");
    });
}
