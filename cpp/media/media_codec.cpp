#include "media_codec.hpp"
#include <atomic>

namespace {
std::atomic_int vpu_written_off{0};
}  // namespace

void media::FormatDeleter::operator()(AVFormatContext* context) const noexcept {
    avformat_close_input(&context);
}
const AVCodec* media::software_decoder(const AVCodecParameters& parameters) {
    return avcodec_find_decoder(parameters.codec_id);
}
bool media::decodable(const AVCodecParameters& parameters) {
    const auto* p = &parameters;
    const AVPixFmtDescriptor* format = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(p->format));
    if(!format)
        return false;
    int depth = format->comp[0].depth;
    if((depth != 8 && depth != 10) || format->log2_chroma_w != 1 || format->log2_chroma_h != 1)
        return false;
    switch(p->codec_id) {
        case AV_CODEC_ID_H264:
        case AV_CODEC_ID_HEVC:
        case AV_CODEC_ID_VP9:
        case AV_CODEC_ID_VP8:
        case AV_CODEC_ID_MPEG4:
        case AV_CODEC_ID_VC1:
        case AV_CODEC_ID_THEORA:
            return software_decoder(parameters) != nullptr;
        default:
            return false;
    }
}
int media::video_stream(AVFormatContext*& file, const char* path) {
    int result = avformat_open_input(&file, path, nullptr, nullptr);
    if(result >= 0)
        result = avformat_find_stream_info(file, nullptr);
    if(result < 0)
        return result;
    int preferred = av_find_best_stream(file, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if(preferred >= 0 && decodable(*file->streams[preferred]->codecpar))
        return preferred;
    for(unsigned i = 0; i < file->nb_streams; ++i) {
        const AVCodecParameters* p = file->streams[i]->codecpar;
        if(p->codec_type == AVMEDIA_TYPE_VIDEO && decodable(*p))
            return static_cast<int>(i);
    }
    return preferred;
}
media::Result media::supported(const char* path) {
    AVFormatContext* opened = nullptr;
    int stream = video_stream(opened, path);
    FormatContext file(opened);
    const bool supported = stream >= 0 && decodable(*file->streams[stream]->codecpar);
    file.reset();
    if(stream < 0 && stream != AVERROR_STREAM_NOT_FOUND)
        return failure("inspect video", stream);
    return {supported, {}};
}
const AVCodec* software_decoder(const AVCodecParameters* p) {
    return media::software_decoder(*p);
}
int decodable(const AVCodecParameters* p) {
    return media::decodable(*p);
}
int video_stream(AVFormatContext** file, const char* path) {
    return media::video_stream(*file, path);
}
int media_vpu_written_off(void) {
    return vpu_written_off.load();
}
void media_vpu_write_off(void) {
    vpu_written_off.store(1);
}
