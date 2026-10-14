#include "panel_pointer.hpp"
#include <cmath>

namespace lumaframevr {
PointerRay ray_of(const Pose& aim) {
    auto direction = rotate(aim.rotation, {0, 0, -1});
    float norm = length(direction);
    if(norm < 1e-9F)
        direction = {0, 0, -1};
    else
        for(auto& value : direction) value /= norm;
    return {aim.position, direction};
}
std::optional<PanelHit> intersect_plane(const PointerRay& ray, const PanelSurface& surface) {
    if(surface.size[0] <= 0 || surface.size[1] <= 0)
        return std::nullopt;
    const auto right = rotate(surface.pose.rotation, {1, 0, 0});
    const auto up = rotate(surface.pose.rotation, {0, 1, 0});
    const auto normal = rotate(surface.pose.rotation, {0, 0, 1});
    const float denominator = dot(ray.direction, normal);
    if(std::abs(denominator) < 1e-6F)
        return std::nullopt;
    Vector3 offset;
    for(std::size_t i = 0; i < offset.size(); ++i) offset[i] = surface.pose.position[i] - ray.origin[i];
    const float distance = dot(offset, normal) / denominator;
    if(distance <= 0)
        return std::nullopt;
    for(std::size_t i = 0; i < offset.size(); ++i) offset[i] = ray.origin[i] + ray.direction[i] * distance - surface.pose.position[i];
    return PanelHit{distance, {dot(offset, right) * 2 / surface.size[0], dot(offset, up) * 2 / surface.size[1]}};
}
std::optional<PanelHit> intersect(const PointerRay& ray, const PanelSurface& surface) {
    auto hit = intersect_plane(ray, surface);
    if(!hit || std::abs(hit->point[0]) > 1 || std::abs(hit->point[1]) > 1)
        return std::nullopt;
    return hit;
}
std::optional<std::size_t> pick(const PointerRay& ray, std::span<const PanelSurface> surfaces, float range) {
    std::optional<std::size_t> selected;
    for(std::size_t i = 0; i < surfaces.size(); ++i) {
        const auto hit = intersect(ray, surfaces[i]);
        if(hit && hit->distance <= range) {
            range = hit->distance;
            selected = i;
        }
    }
    return selected;
}
}  // namespace lumaframevr
