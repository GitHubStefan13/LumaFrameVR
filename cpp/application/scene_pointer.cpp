#include "application_scene.hpp"
#include <algorithm>
namespace lumaframevr::application {
std::optional<Pose> Scene::aim(unsigned hand) const {
    if(hand > 1)
        return {};
    return hands_.aim_poses[hand] ? hands_.aim_poses[hand] : hands_.grip_poses[hand];
}
std::optional<std::pair<std::size_t, PanelHit>> Scene::nearest(unsigned hand) const {
    const auto pose = aim(hand);
    if(!pose)
        return {};
    std::optional<std::pair<std::size_t, PanelHit>> result;
    for(std::size_t index = 0; index < panels_.size(); ++index) {
        if(panels_[index].kind == PanelKind::startup)
            continue;
        auto point = intersect(ray_of(*pose), panels_[index].surface);
        if(point && point->distance <= 8 && (!result || point->distance < result->second.distance))
            result = std::pair{index, *point};
    }
    return result;
}
Hit Scene::hit(const ScenePanel& panel, PanelHit point) const {
    const int x = static_cast<int>((point.point[0] + 1) * panel.pixels[0] / 2);
    const int y = static_cast<int>((1 - point.point[1]) * panel.pixels[1] / 2);
    return {panel.kind, panel.kind == PanelKind::browser ? 1200 : panel.pixels[0], panel.pixels[1], x, y, point.point, panel.slot};
}
std::optional<Hit> Scene::pick(unsigned hand) const {
    const auto point = nearest(hand);
    if(!point)
        return {};
    auto result = hit(panels_[point->first], point->second);
    if(result.panel == PanelKind::browser && result.x >= 1200)
        return {};
    return result;
}
std::optional<Hit> Scene::drag(unsigned hand, PanelKind kind) const {
    const auto pose = aim(hand);
    if(!pose)
        return {};
    for(const auto& panel : panels_) {
        if(panel.kind != kind || panel.owner)
            continue;
        auto point = intersect_plane(ray_of(*pose), panel.surface);
        if(point && point->distance <= 8)
            return hit(panel, *point);
    }
    return {};
}
SceneOverlays Scene::overlay_payloads(Pose eye, std::array<float, 4> fov) const {
    SceneOverlays output;
    for(unsigned hand = 0; hand < 2; ++hand) {
        if(const auto pose = aim(hand)) {
            const auto ray = ray_of(*pose);
            const auto point = nearest(hand);
            const float distance = point ? point->second.distance : 8;
            OverlayPayload beam;
            for(unsigned axis = 0; axis < 3; ++axis) {
                beam.values[0][axis] = ray.origin[axis] - eye.position[axis];
                beam.values[1][axis] = ray.origin[axis] + ray.direction[axis] * distance - eye.position[axis];
            }
            beam.values[2] = point ? std::array<float, 4>{1, .88F, .42F, .85F} : std::array<float, 4>{.55F, .62F, .70F, .85F};
            beam.values[3] = conjugate(eye.rotation);
            beam.values[4] = fov;
            beam.values[5] = {.003F, 1, point ? .011F : 0, .65F};
            output.beams.push_back(beam);
        }
        if(const auto& pose = hands_.grip_poses[hand]) {
            OverlayPayload body;
            for(unsigned axis = 0; axis < 3; ++axis) body.values[0][axis] = pose->position[axis] - eye.position[axis];
            body.values[1] = pose->rotation;
            body.values[2] = {.028F, .026F, .075F, .020F};
            body.values[3] = {.16F, .17F, .20F, 1};
            body.values[4] = conjugate(eye.rotation);
            body.values[5] = fov;
            output.controllers.push_back(body);
        }
    }
    return output;
}
}  // namespace lumaframevr::application
