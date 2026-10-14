#pragma once
#include <memory>
#include <sys/resource.h>

struct AVCodecContext;
struct Media;

namespace media {
struct CodecDeleter {
    void operator()(AVCodecContext* context) const noexcept;
};
using CodecContext = std::unique_ptr<AVCodecContext, CodecDeleter>;
int open_decoder(Media& media);
int open_software_decoder(Media& media);

// A preview or thumbnail decode has to lose the CPU race with the render thread: FFmpeg's automatic
// thread count is every core, and one 8K HEVC frame decoded that way stalls the file viewer for a
// second. PreviewDecode marks the worker thread that decodes, so a software decoder opened on it is
// bounded to preview_decoder_threads and the thread drops below the render thread's priority.
// Playback never marks a thread, so its decoding keeps the automatic count.
constexpr int preview_decoder_threads = 2;

inline bool& on_preview_decode_thread() {
    static thread_local bool value = false;
    return value;
}
inline int software_decoder_thread_count() {
    return on_preview_decode_thread() ? preview_decoder_threads : 0;
}
class PreviewDecode {
   public:
    PreviewDecode() noexcept : previous_(on_preview_decode_thread()) {
        on_preview_decode_thread() = true;
        setpriority(PRIO_PROCESS, 0, 19);
    }
    ~PreviewDecode() { on_preview_decode_thread() = previous_; }
    PreviewDecode(const PreviewDecode&) = delete;
    PreviewDecode& operator=(const PreviewDecode&) = delete;

   private:
    bool previous_;
};
}  // namespace media
