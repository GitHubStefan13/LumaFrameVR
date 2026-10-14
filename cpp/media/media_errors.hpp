#pragma once
#include "media_result.hpp"

namespace media {
Result unsupported_video(const AVCodecParameters* parameters, const char* path);
Result startup_failure(const AVCodecParameters* parameters, const char* path, const char* operation, int code);
}  // namespace media
