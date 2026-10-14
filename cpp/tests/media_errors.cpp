#include "test_registry.hpp"
#include "test_assertions.hpp"
#include "media_codec.hpp"
#include "media_errors.hpp"
#include <cerrno>
#include <string_view>

namespace {
AVCodecParameters describe(AVCodecID codec, AVPixelFormat format, int width, int height, int profile = 0) {
    AVCodecParameters parameters{};
    parameters.codec_type = AVMEDIA_TYPE_VIDEO;
    parameters.codec_id = codec;
    parameters.format = format;
    parameters.width = width;
    parameters.height = height;
    parameters.profile = profile;
    return parameters;
}
void has(const media::Result& result, std::string_view text) {
    require(result.status == -1 && result.error.find(text) != std::string::npos, "diagnostic contains " + std::string(text));
}
}  // namespace

int lumaframevr::tests::media_errors(int, char**) {
    return acceptance_main([] {
        auto parameters = describe(AV_CODEC_ID_H264, AV_PIX_FMT_YUV420P, 1920, 1080);
        require(std::string_view(decoder_name(&parameters)) == "h264_v4l2m2m", "H264 hardware name");
        parameters = describe(AV_CODEC_ID_HEVC, AV_PIX_FMT_YUV420P10LE, 3840, 2160, 2);
        require(std::string_view(decoder_name(&parameters)) == "hevc_v4l2m2m", "HEVC Main10 hardware name");
        parameters = describe(AV_CODEC_ID_VP9, AV_PIX_FMT_YUV420P, 1920, 1080);
        require(std::string_view(decoder_name(&parameters)) == "vp9_v4l2m2m", "VP9 hardware name");
        parameters = describe(AV_CODEC_ID_VP8, AV_PIX_FMT_YUV420P, 1920, 1080);
        require(!decoder_name(&parameters), "VP8 has no hardware name");
        parameters = describe(AV_CODEC_ID_HEVC, AV_PIX_FMT_YUV420P12LE, 3840, 2160);
        require(!decoder_name(&parameters), "12-bit HEVC has no hardware name");
        parameters = describe(AV_CODEC_ID_AV1, AV_PIX_FMT_YUV420P, 1920, 1080);
        const auto unsupported = ::media::unsupported_video(&parameters, "/videos/clip.mkv");
        has(unsupported, "Unsupported video");
        has(unsupported, "av1");
        has(unsupported, "clip.mkv");
        parameters = describe(AV_CODEC_ID_HEVC, AV_PIX_FMT_YUV420P10LE, 7680, 4320, 2);
        const auto startup = ::media::startup_failure(&parameters, "/videos/main10-8k60.mp4", "open native decoder", AVERROR(ENOMEM));
        for(auto field : {"Video decoder start failed", "open native decoder", "-12", "hevc", "7680x4320", "depth=10", "main10-8k60.mp4", "not in codec support"}) has(startup, field);
        const auto same_unsupported = ::media::unsupported_video(&parameters, "same.mp4");
        const auto same_startup = ::media::startup_failure(&parameters, "same.mp4", "open native decoder", AVERROR(EINVAL));
        has(same_unsupported, "Unsupported video");
        has(same_startup, "Video decoder start failed");
        require(startup.error.find("Unsupported video") == std::string::npos && same_startup.error.find("Unsupported video") == std::string::npos, "startup failure never labels a codec unsupported");
        require(same_unsupported.error.find("Video decoder start failed") == std::string::npos, "unsupported failure never labels startup");
        const auto failed = ::media::open("/nonexistent/lumaframevr-media-errors.mp4", true);
        require(std::holds_alternative<::media::Error>(failed), "typed open returns Error instead of Session");
        const auto& error = std::get<::media::Error>(failed);
        require(error.code == AVERROR(ENOENT) && error.message.find("lumaframevr-media-errors.mp4") != std::string::npos, "typed Error retains code and path");
    });
}
