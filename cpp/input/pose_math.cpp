#include "pose_math.hpp"
#include <algorithm>
#include <stdexcept>

namespace lumaframevr {
Quaternion normalize(Quaternion value) {
    const float norm = length(value);
    if(!std::isfinite(norm) || norm < 1e-12F)
        throw std::invalid_argument("Invalid pose quaternion");
    for(auto& item : value) item /= norm;
    return value;
}
Quaternion conjugate(Quaternion value) {
    return {-value[0], -value[1], -value[2], value[3]};
}
Quaternion multiply(Quaternion a, Quaternion b) {
    const auto [x, y, z, w] = a;
    const auto [u, v, s, t] = b;
    return {w * u + x * t + y * s - z * v, w * v - x * s + y * t + z * u, w * s + x * v - y * u + z * t, w * t - x * u - y * v - z * s};
}
Quaternion interpolate(Quaternion a, Quaternion b, float weight) {
    float product = dot(a, b);
    if(product < 0) {
        for(auto& value : b) value = -value;
        product = -product;
    }
    if(product > 0.9995F)
        return normalize(mix(a, b, weight));
    const auto angle = std::acos(std::clamp(product, 0.0F, 1.0F));
    const float left = std::sin((1 - weight) * angle), right = std::sin(weight * angle);
    Quaternion result;
    for(std::size_t i = 0; i < result.size(); ++i) result[i] = left * a[i] + right * b[i];
    return normalize(result);
}
Vector3 angular_velocity(Quaternion previous, Quaternion current, float seconds) {
    auto delta = multiply(current, conjugate(previous));
    if(delta[3] < 0)
        for(auto& value : delta) value = -value;
    Vector3 axis{delta[0], delta[1], delta[2]};
    const float sine = length(axis);
    const float scale = sine > 1e-6F ? 2 * std::atan2(sine, delta[3]) / sine : 2;
    for(auto& value : axis) value *= scale / seconds;
    return axis;
}
Vector3 rotate(Quaternion rotation, Vector3 point) {
    auto result = multiply(multiply(rotation, {point[0], point[1], point[2], 0}), conjugate(rotation));
    return {result[0], result[1], result[2]};
}
}  // namespace lumaframevr
