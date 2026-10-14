#pragma once
#include "media_types.hpp"
#include "media_trace.h"
#include <array>
#include <memory>
#include <string>
#include <variant>

struct Media;

namespace media {
class Frame;
struct Error {
    int code;
    std::string message;
};
struct Result {
    int status;
    std::string error;
};
struct AudioTick {
    Result result;
    double clock = -1;
    double limit = 0;
    int done = 0;
};
struct SessionDeleter {
    void operator()(Media* media) const noexcept;
};
class Session {
   public:
    explicit Session(Media* media) noexcept : media_(media) {}
    Session(Session&&) noexcept = default;
    Session& operator=(Session&&) noexcept = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Media* get() const noexcept { return media_.get(); }
    Result next(Frame& frame);
    Result reconfigure(bool released);
    Result seek(double seconds);
    MediaBuffer buffer(bool prefill);
    MediaInfo info() const;
    double duration() const;
    std::array<int, 3> metadata() const;
    Result preview();
    void stop();
    void profile(bool continuous);
    MediaTrace trace();
    Result enable_audio();
    AudioTick audio(bool paused);
    bool has_audio() const;
    Result volume(float volume, bool muted);

   private:
    std::unique_ptr<Media, SessionDeleter> media_;
};
using OpenResult = std::variant<Session, Error>;
OpenResult open(const char* path, bool software_only = false);
}  // namespace media
