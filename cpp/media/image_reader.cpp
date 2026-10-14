#include "image_conversion.hpp"
#include <chrono>
#include <memory>
#include <stdexcept>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}
namespace lumaframevr {
namespace {
struct Deadline {
    const std::function<bool()>& cancelled;
    std::chrono::steady_clock::time_point until{std::chrono::steady_clock::now() + std::chrono::seconds(10)};
    bool stop() const { return (cancelled && cancelled()) || std::chrono::steady_clock::now() >= until; }
};
int interrupt(void* value) {
    return static_cast<Deadline*>(value)->stop();
}
void check(int code, const char* operation) {
    if(code >= 0)
        return;
    char detail[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(code, detail, sizeof(detail));
    throw std::runtime_error(std::string(operation) + ": " + detail);
}
struct Input {
    AVFormatContext* value{avformat_alloc_context()};
    ~Input() { avformat_close_input(&value); }
};
struct Codec {
    AVCodecContext* value{};
    ~Codec() { avcodec_free_context(&value); }
};
struct Frame {
    AVFrame* value{av_frame_alloc()};
    ~Frame() { av_frame_free(&value); }
};
struct Packet {
    AVPacket* value{av_packet_alloc()};
    ~Packet() { av_packet_free(&value); }
};
struct ProbeOptions {
    std::vector<AVDictionary*> streams;
    explicit ProbeOptions(unsigned count) : streams(count) {
        for(auto& options : streams) {
            av_dict_set_int(&options, "max_pixels", maximum_image_pixels, 0);
            av_dict_set(&options, "threads", "1", 0);
        }
    }
    ~ProbeOptions() {
        for(auto& options : streams) av_dict_free(&options);
    }
};
bool image_codec(AVCodecID id) {
    switch(id) {
        case AV_CODEC_ID_PNG:
        case AV_CODEC_ID_APNG:
        case AV_CODEC_ID_MJPEG:
        case AV_CODEC_ID_BMP:
        case AV_CODEC_ID_WEBP:
        case AV_CODEC_ID_GIF:
        case AV_CODEC_ID_TIFF:
            return true;
        default:
            return false;
    }
}
void dimensions(int width, int height) {
    if(width <= 0 || height <= 0 || width > 16384 || height > 16384 || static_cast<std::int64_t>(width) * height > maximum_image_pixels)
        throw std::runtime_error("Image dimensions exceed the 64-megapixel/16384-edge limit");
}
}  // namespace
ImagePixels read_image(const std::filesystem::path& path, const std::function<bool()>& cancelled) {
    if(!std::filesystem::is_regular_file(path))
        throw std::runtime_error("Image source is not a regular file: " + path.string());
    if(std::filesystem::file_size(path) > 128ULL * 1024 * 1024)
        throw std::runtime_error("Image file exceeds the 128-MiB limit");
    Deadline deadline{cancelled};
    if(deadline.stop())
        throw std::runtime_error("Image load cancelled");
    Input input;
    if(!input.value)
        throw std::bad_alloc();
    input.value->interrupt_callback = {interrupt, &deadline};
    AVDictionary* options{};
    av_dict_set(&options, "protocol_whitelist", "file", 0);
    av_dict_set(&options, "format_whitelist", "image2,image2pipe,png_pipe,jpeg_pipe,bmp_pipe,webp_pipe,tiff_pipe,gif,apng", 0);
    av_dict_set(&options, "pattern_type", "none", 0);
    const auto status = avformat_open_input(&input.value, path.c_str(), nullptr, &options);
    av_dict_free(&options);
    check(status, "Open image");
    ProbeOptions probe_options(input.value->nb_streams);
    check(avformat_find_stream_info(input.value, probe_options.streams.data()), "Read image information");
    const int stream_index = av_find_best_stream(input.value, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    check(stream_index, "Find image stream");
    const auto* parameters = input.value->streams[stream_index]->codecpar;
    if(!image_codec(parameters->codec_id))
        throw std::runtime_error("Selected file is not a supported still image");
    dimensions(parameters->width, parameters->height);
    const auto* decoder = avcodec_find_decoder(parameters->codec_id);
    if(!decoder)
        throw std::runtime_error("Image decoder is not present in this build");
    Codec codec{avcodec_alloc_context3(decoder)};
    if(!codec.value)
        throw std::bad_alloc();
    check(avcodec_parameters_to_context(codec.value, parameters), "Configure image decoder");
    codec.value->max_pixels = maximum_image_pixels;
    codec.value->thread_count = 1;
    check(avcodec_open2(codec.value, decoder, nullptr), "Open image decoder");
    Frame frame;
    Packet packet;
    if(!frame.value || !packet.value)
        throw std::bad_alloc();
    bool draining = false;
    for(;;) {
        if(deadline.stop())
            throw std::runtime_error("Image load cancelled or timed out");
        const int received = avcodec_receive_frame(codec.value, frame.value);
        if(received == 0)
            break;
        if(received == AVERROR_EOF)
            throw std::runtime_error("Image has no decoded frame");
        if(received != AVERROR(EAGAIN))
            check(received, "Decode image");
        if(draining)
            throw std::runtime_error("Image has no decoded frame");
        int read;
        do {
            av_packet_unref(packet.value);
            read = av_read_frame(input.value, packet.value);
        } while(read >= 0 && packet.value->stream_index != stream_index && !deadline.stop());
        if(read == AVERROR_EOF) {
            draining = true;
            check(avcodec_send_packet(codec.value, nullptr), "Drain image decoder");
        } else {
            check(read, "Read image packet");
            check(avcodec_send_packet(codec.value, packet.value), "Send image packet");
        }
    }
    dimensions(frame.value->width, frame.value->height);
    if(frame.value->decode_error_flags || (frame.value->flags & AV_FRAME_FLAG_CORRUPT))
        throw std::runtime_error("Corrupt image frame");
    return orient_image(image_rgba(*frame.value, [&] { return deadline.stop(); }), *frame.value);
}
}  // namespace lumaframevr
