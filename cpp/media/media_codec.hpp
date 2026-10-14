#pragma once
#include "media_result.hpp"
#include <memory>

namespace media {
struct FormatDeleter {
    void operator()(AVFormatContext* context) const noexcept;
};
using FormatContext = std::unique_ptr<AVFormatContext, FormatDeleter>;
const AVCodec* software_decoder(const AVCodecParameters& parameters);
bool decodable(const AVCodecParameters& parameters);
int video_stream(AVFormatContext*& file, const char* path);
Result supported(const char* path);
}  // namespace media
