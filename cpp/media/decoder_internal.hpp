#pragma once
#include "decoder_session.hpp"
#include <stdexcept>
extern "C" {
#include <libavutil/pixfmt.h>
}

#include "media_session.hpp"
#include "media_owned_frame.hpp"

namespace lumaframevr {
struct DecoderSession::Session {
    explicit Session(media::Session value) : native(std::move(value)) {}
    media::Session native;
};
struct RetainedFrame::Storage {
    explicit Storage(media::Frame value) : pixels(std::move(value)) {}
    media::Frame pixels;
};
inline void check_native(int result, const char* message) {
    if(result < 0)
        throw std::runtime_error(message[0] ? message : "Native media operation failed without detail");
}
}  // namespace lumaframevr
