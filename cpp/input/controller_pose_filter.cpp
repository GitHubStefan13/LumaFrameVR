#include "controller_pose_filter.hpp"
#include <numbers>

namespace lumaframevr {
namespace {
float alpha(float cutoff, float seconds) {
    return 1 / (1 + 1 / (2 * std::numbers::pi_v<float> * cutoff * seconds));
}
}  // namespace
std::optional<Pose> ControllerPoseFilter::peek(std::int64_t xr_time) const {
    if(!previous_ || previous_->time != xr_time)
        return std::nullopt;
    return previous_->filtered;
}
std::optional<Pose> ControllerPoseFilter::update(std::optional<Pose> pose, std::int64_t xr_time) {
    if(!pose) {
        previous_.reset();
        return std::nullopt;
    }
    pose->rotation = normalize(pose->rotation);
    Sample next{xr_time, *pose, *pose, {}, {}};
    if(previous_) {
        const auto& previous = *previous_;
        const float seconds = static_cast<float>(xr_time - previous.time) * 1e-9F;
        if(seconds > 0 && seconds <= 0.25F) {
            Vector3 velocity;
            for(std::size_t i = 0; i < velocity.size(); ++i) velocity[i] = (pose->position[i] - previous.raw.position[i]) / seconds;
            next.velocity = mix(previous.velocity, velocity, alpha(1, seconds));
            next.angular_velocity = mix(previous.angular_velocity, angular_velocity(previous.raw.rotation, pose->rotation, seconds), alpha(1, seconds));
            next.filtered.position = mix(previous.filtered.position, pose->position, alpha(4 + 20 * length(next.velocity), seconds));
            next.filtered.rotation = interpolate(previous.filtered.rotation, pose->rotation, alpha(2 + 3 * length(next.angular_velocity), seconds));
        }
    }
    previous_ = next;
    return next.filtered;
}
}  // namespace lumaframevr
