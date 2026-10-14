#pragma once
#include <array>
#include <cmath>
#include <cstddef>

namespace lumaframevr {
using Vector3 = std::array<float, 3>;
using Quaternion = std::array<float, 4>;
struct Pose {
    Vector3 position{};
    Quaternion rotation{0, 0, 0, 1};
    bool operator==(const Pose&) const = default;
};
template <std::size_t N>
float dot(const std::array<float, N>& a, const std::array<float, N>& b) {
    float sum = 0;
    for(std::size_t i = 0; i < N; ++i) sum += a[i] * b[i];
    return sum;
}
template <std::size_t N>
float length(const std::array<float, N>& vector) {
    return std::sqrt(dot(vector, vector));
}
template <std::size_t N>
std::array<float, N> mix(const std::array<float, N>& a, const std::array<float, N>& b, float weight) {
    std::array<float, N> result;
    for(std::size_t i = 0; i < N; ++i) result[i] = a[i] + weight * (b[i] - a[i]);
    return result;
}
Quaternion normalize(Quaternion value);
Quaternion conjugate(Quaternion value);
Quaternion multiply(Quaternion a, Quaternion b);
Quaternion interpolate(Quaternion a, Quaternion b, float weight);
Vector3 angular_velocity(Quaternion previous, Quaternion current, float seconds);
Vector3 rotate(Quaternion rotation, Vector3 point);
}  // namespace lumaframevr
