#pragma once
#include "pose_math.hpp"
#include <cstdint>
#include <optional>

namespace lumaframevr {
class ControllerPoseFilter {
   public:
    std::optional<Pose> update(std::optional<Pose> pose, std::int64_t xr_time);
    std::optional<Pose> peek(std::int64_t xr_time) const;

   private:
    struct Sample {
        std::int64_t time;
        Pose filtered, raw;
        Vector3 velocity{}, angular_velocity{};
    };
    std::optional<Sample> previous_;
};
}  // namespace lumaframevr
