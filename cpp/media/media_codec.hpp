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
// The codecs this player knows how to decode, independent of whether this build of FFmpeg carries
// a decoder for them and of the stream's pixel format. The refusal message asks this so it can say
// which of the two is the reason: the codec, or the format.
bool supported_codec(AVCodecID codec_id);
// Whether this build can decode AV1 in software. FFmpeg 7's native "av1" entry cannot: it is a
// hardware-accelerator shell until native decode is implemented, so only libdav1d or libaom-av1
// make AV1 playable here.
bool av1_software_decoder_available();
int video_stream(AVFormatContext*& file, const char* path);
Result supported(const char* path);
}  // namespace media
