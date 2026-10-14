#pragma once
#include "v4l2_decoder.hpp"
#include <functional>
#include <memory>
#include <string_view>

struct Media;
struct AVFrame;
struct AVCodecParameters;
namespace media {
struct Result;
struct V4l2Frame;
// Whether our own V4L2 queue can carry this stream (codec, depth and chroma layout). The
// decoder policy asks before choosing it, so an unsupported stream never opens the device.
bool v4l2_accepts(const AVCodecParameters& parameters);
// Preserve the queue layout, timestamp and lease at the DRM frame boundary.
void export_v4l2_frame(Media& media, V4l2Frame decoded);
// Frame-local provenance remains valid across clones and decoder replacement.
bool v4l2_source_layout(const AVFrame& frame);
// FFmpeg owns demux, bitstream conversion, parsing and the stream description;
// this adapter alone owns the opt-in V4L2 decode queue. A replacement decoder is
// built only after the previous queue's retained frames have retired, because
// their leases keep its buffers allocated and the driver refuses to allocate
// over them.
class V4l2Playback {
   public:
    using Opener = std::function<std::unique_ptr<V4l2Decoder>(V4l2Stream)>;
    explicit V4l2Playback(Media& media, Opener opener = {});
    void reset(Media& media);
    ~V4l2Playback();
    Result receive(Media& media);

   private:
    Result receive_once(Media& media);
    struct State;
    std::unique_ptr<State> state_;
};
int open_v4l2_decoder(Media& media);
int reset_v4l2_decoder(Media& media);
int fallback_v4l2(Media& media, std::string_view reason, bool rewind);
}  // namespace media
