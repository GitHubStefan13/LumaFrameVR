#include "seek_repeat.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lumaframevr {
namespace {
constexpr std::array<unsigned, 6> speeds{1, 2, 4, 8, 16, 32};
}
unsigned SeekRepeat::speed() const {
    if(direction_ == 0)
        return 0;
    return speeds[std::min(static_cast<std::size_t>(std::max(0.0F, rung_)), speeds.size() - 1)];
}
int SeekRepeat::update(int direction, double monotonic_seconds) {
    if(direction < -1 || direction > 1 || !std::isfinite(monotonic_seconds))
        throw std::invalid_argument("Invalid seek-repeat input");
    if(direction != direction_) {
        direction_ = direction;
        rung_ = 0;
        pending_ = 0;
        pressed_.reset();
        last_.reset();
        if(!direction)
            return 0;
        pressed_ = last_ = monotonic_seconds;
        return direction * 10;
    }
    if(!direction)
        return 0;
    const auto previous = std::exchange(last_, monotonic_seconds);
    if(!previous)
        return 0;
    const auto elapsed = std::max(0.0, monotonic_seconds - *previous);
    if(elapsed > 0.25)
        return 0;
    const auto held = std::max(0.0, monotonic_seconds - pressed_.value_or(monotonic_seconds));
    rung_ = held < 0.4 ? 0 : std::min(static_cast<float>((held - 0.4) / 0.45), 5.0F);
    pending_ += elapsed * speed();
    const auto whole = std::floor(pending_);
    pending_ -= whole;
    return direction * static_cast<int>(whole);
}
}  // namespace lumaframevr
