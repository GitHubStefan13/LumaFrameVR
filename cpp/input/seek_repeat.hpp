#pragma once
#include <optional>

namespace lumaframevr {
class SeekRepeat {
   public:
    int update(int direction, double monotonic_seconds);
    unsigned speed() const;

   private:
    int direction_{};
    float rung_{};
    std::optional<double> pressed_, last_;
    double pending_{};
};
}  // namespace lumaframevr
