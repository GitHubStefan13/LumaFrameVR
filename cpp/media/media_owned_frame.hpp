#pragma once
#include "media_types.hpp"
#include <utility>

namespace media {
void release(FrameData& frame) noexcept;
class Frame {
   public:
    Frame() = default;
    ~Frame() { reset(); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    Frame(Frame&& other) noexcept : value_(std::exchange(other.value_, {})) {}
    Frame& operator=(Frame&& other) noexcept {
        if(this != &other) {
            reset();
            value_ = std::exchange(other.value_, {});
        }
        return *this;
    }
    FrameData& get() noexcept { return value_; }
    const FrameData& get() const noexcept { return value_; }

   private:
    void reset() noexcept {
        if(value_.owner)
            release(value_);
    }
    FrameData value_{};
};
}  // namespace media
