#include "media_errors.hpp"
#include <array>
#include <cstdio>

namespace {
template <class... Arguments>
std::string format_message(const char* format, Arguments... arguments) {
    const int length = std::snprintf(nullptr, 0, format, arguments...);
    std::string message(static_cast<size_t>(length), '\0');
    std::snprintf(message.data(), message.size() + 1, format, arguments...);
    return message;
}
}  // namespace

const char* decoder_name(const AVCodecParameters* p) {
    const AVPixFmtDescriptor* format = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(p->format));
    if(!format || format->log2_chroma_w != 1 || format->log2_chroma_h != 1 || (format->comp[0].depth != 8 && !(p->codec_id == AV_CODEC_ID_HEVC && format->comp[0].depth == 10)))
        return nullptr;
    return p->codec_id == AV_CODEC_ID_H264 ? "h264_v4l2m2m" : p->codec_id == AV_CODEC_ID_HEVC ? "hevc_v4l2m2m" : p->codec_id == AV_CODEC_ID_VP9 ? "vp9_v4l2m2m" : nullptr;
}

media::Result media::unsupported_video(const AVCodecParameters* p, const char* path) {
    const AVPixFmtDescriptor* pixel = p ? av_pix_fmt_desc_get(static_cast<AVPixelFormat>(p->format)) : nullptr;
    const char* profile = p ? avcodec_profile_name(p->codec_id, p->profile) : nullptr;
    std::array<char, 16> number{};
    if(!profile && p) {
        std::snprintf(number.data(), number.size(), "%d", p->profile);
        profile = number.data();
    }
    const AVCodec* software = p ? software_decoder(p) : nullptr;
    return {-1, format_message("Unsupported video: codec=%s, profile=%s, pixel_format=%s, %dx%d, depth=%d "
                               "(supported: h264, hevc, vp8, vp9, mpeg4, vc1 and theora, 8-bit; "
                               "av1 is not decodable on this build - it needs libdav1d or an AV1 hwaccel, "
                               "neither of which is present); "
                               "hardware decoder=%s, software decoder=%s; file=%s",
                               p ? avcodec_get_name(p->codec_id) : "none", profile ? profile : "unknown", pixel ? pixel->name : "unknown", p ? p->width : 0, p ? p->height : 0,
                               pixel ? pixel->comp[0].depth : 0, p && decoder_name(p) ? decoder_name(p) : "none", software ? software->name : "none", path)};
}

media::Result media::startup_failure(const AVCodecParameters* p, const char* path, const char* operation, int code) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> detail{};
    std::array<char, 32> profile{};
    const AVPixFmtDescriptor* pixel = p ? av_pix_fmt_desc_get(static_cast<AVPixelFormat>(p->format)) : nullptr;
    const char* name = p ? avcodec_profile_name(p->codec_id, p->profile) : nullptr;
    if(!name && p) {
        std::snprintf(profile.data(), profile.size(), "%d", p->profile);
        name = profile.data();
    }
    av_strerror(code, detail.data(), detail.size());
    return {-1, format_message("Video decoder start failed: %s: %s (%d); file=%s\n"
                               "codec=%s (%s), pixel_format=%s, %dx%d, depth=%d; this codec is "
                               "supported - the failure is in starting or driving the decoder, "
                               "not in codec support",
                               operation ? operation : "open decoder", detail.data(), code, path, p ? avcodec_get_name(p->codec_id) : "none", name ? name : "unknown", pixel ? pixel->name : "unknown",
                               p ? p->width : 0, p ? p->height : 0, pixel ? pixel->comp[0].depth : 0)};
}

int unsupported_video(AVCodecParameters* p, const char* path, char* error, size_t capacity) {
    try {
        return media::export_result(media::unsupported_video(p, path), error, capacity);
    } catch(const std::bad_alloc&) {
        return media_error(error, capacity, "describe unsupported video", AVERROR(ENOMEM));
    }
}
int startup_failure(AVCodecParameters* p, const char* path, const char* operation, int code, char* error, size_t capacity) {
    try {
        return media::export_result(media::startup_failure(p, path, operation, code), error, capacity);
    } catch(const std::bad_alloc&) {
        return media_error(error, capacity, "describe decoder failure", AVERROR(ENOMEM));
    }
}
