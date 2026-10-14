#pragma once
#include <cstdint>

namespace media {
struct DmaBuf {
    int fd;
    uint32_t format;
    uint64_t size, modifier, offsets[2], pitches[2], pool;
    int width, height, crop[4], chroma_location, stream, transfer, primaries;
    // A negotiated source layout must not be replaced by a Vulkan-derived one.
    bool source_layout = false;
};
struct ColorInfo {
    int transfer, primaries, chroma_location, bits;
    float mastering_min_nits, mastering_max_nits;
    float max_cll, max_fall;
    float mastering_xy[8];
};
struct FrameData {
    void* owner;
    int width, height, colorspace, full_range;
    float sample_aspect_ratio;
    double pts;
    DmaBuf dmabuf;
    int software, planes, format;
    uint8_t* data[3];
    int linesize[3];
    ColorInfo color;
};
struct MediaInfo {
    char name[32];
    int hardware;
    int width, height, bits;
    double fps;
    double bitrate_mbps;
};
struct MediaBuffer {
    int64_t video_us, audio_us;
    uint64_t bytes;
    int ready, limited, waiting;
};
}  // namespace media
