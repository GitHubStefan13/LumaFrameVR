#pragma once
#include "pose_math.hpp"
#include <optional>
#include <span>

namespace lumaframevr {
struct PanelSurface {
    Pose pose;
    std::array<float, 2> size;
};
struct PointerRay {
    Vector3 origin, direction;
};
struct PanelHit {
    float distance;
    std::array<float, 2> point;
};
PointerRay ray_of(const Pose& aim);
std::optional<PanelHit> intersect_plane(const PointerRay& ray, const PanelSurface& surface);
std::optional<PanelHit> intersect(const PointerRay& ray, const PanelSurface& surface);
std::optional<std::size_t> pick(const PointerRay& ray, std::span<const PanelSurface> surfaces, float range = 8);
}  // namespace lumaframevr
