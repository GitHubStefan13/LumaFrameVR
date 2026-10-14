#include "application_scene.hpp"
#include "branding.hpp"
#include <numbers>
namespace lumaframevr::application {
namespace {
constexpr float playback_panel_distance = 2.6F;
}
Pose compose(Pose parent, Pose child) {
    auto offset = rotate(parent.rotation, child.position);
    for(unsigned axis = 0; axis < 3; ++axis) child.position[axis] = parent.position[axis] + offset[axis];
    child.rotation = multiply(parent.rotation, child.rotation);
    return child;
}
void Scene::update(const SceneSnapshot& app, const ControllerState& hands, std::optional<Pose> head, double) {
    hands_ = hands;
    panels_.clear();
    const auto visible = app.panels;
    const auto append = [this](PanelKind kind, Pose pose, std::array<int, 2> pixels, float width, unsigned slot, std::optional<unsigned> owner) {
        panels_.push_back({kind, {pose, {width * pixels[0] / 792, width * pixels[1] / 792}}, pixels, slot, owner});
    };
    for(unsigned slot = 0; slot < 3; ++slot) {
        const auto kind = visible[slot];
        if(slot < 2) {
            if(kind != anchored_kinds_[slot])
                anchors_[slot].reset();
            anchored_kinds_[slot] = kind;
            if(!kind)
                anchors_[slot].reset();
            else if(!anchors_[slot])
                anchors_[slot] = hands.grip_poses[slot];
        }
        if(!kind)
            continue;
        const std::array<int, 2> pixels = *kind == PanelKind::browser ? std::array<int, 2>{1520, app.playing ? 368 : 800}
                                                                      : std::array<int, 2>{792, ui::branding_header_height + 58 + 36 * static_cast<int>(app.information_rows + 3)};
        if(slot < 2) {
            if(!anchors_[slot])
                continue;
            Pose offset{{0, .3F, -.3F}, {-std::sin(std::numbers::pi_v<float> / 6), 0, 0, std::cos(std::numbers::pi_v<float> / 6)}};
            append(*kind, compose(*anchors_[slot], offset), pixels, *kind == PanelKind::browser ? .42F * 1160 / 1520 : .42F, slot, slot);
        } else if(head) {
            Pose offset;
            offset.position = *kind == PanelKind::browser ? Vector3{0, -.05F, -6} : Vector3{0, -.43F, -playback_panel_distance};
            append(*kind, compose(*head, offset), pixels, *kind == PanelKind::browser ? 3.84F * 1160 / 1520 : ui::advanced_reference_width, slot, {});
        }
    }
    if(app.transport_visible) {
        Pose pose;
        pose.position = {app.bar_offset[0], -(0.62F / 1.75F) * .6F + app.bar_offset[1], -playback_panel_distance};
        append(PanelKind::transport, pose, {1160, 400}, ui::advanced_reference_width, 3, {});
    }
    if(head && app.startup_visible) {
        Pose offset;
        offset.position = {0, 0, -playback_panel_distance};
        append(PanelKind::startup, compose(*head, offset), {1024, 576}, 1.2F, 6, {});
    }
    if(!app.playing)
        return;
    for(unsigned index = 0; index < 2; ++index) {
        const auto kind = static_cast<ui::AdvancedKind>(index);
        if(app.dialogs[index].has_value())
            append(index == 0 ? PanelKind::settings : PanelKind::rotation, ui::advanced_pose(kind, app.bar_offset, *app.dialogs[index]), index == 0 ? ui::settings_size : ui::alignment_size,
                   ui::advanced_reference_width, 4 + index, {});
    }
}
}  // namespace lumaframevr::application
