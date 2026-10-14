#pragma once
#include "application_controller.hpp"
#include "panel_pointer.hpp"
namespace lumaframevr::application {
struct ScenePanel {
    PanelKind kind;
    PanelSurface surface;
    std::array<int, 2> pixels;
    unsigned slot;
    std::optional<unsigned> owner;
};
struct OverlayPayload {
    std::array<std::array<float, 4>, 6> values{};
};
static_assert(sizeof(OverlayPayload) == 96);
struct SceneOverlays {
    std::vector<OverlayPayload> beams, controllers;
};
Pose compose(Pose parent, Pose child);
class Scene {
   public:
    void update(const SceneSnapshot& app, const ControllerState& hands, std::optional<Pose> head, double now);
    const std::vector<ScenePanel>& panels() const { return panels_; }
    std::optional<Hit> pick(unsigned hand) const;
    std::optional<Hit> drag(unsigned hand, PanelKind kind) const;
    SceneOverlays overlay_payloads(Pose eye, std::array<float, 4> fov) const;
    const std::array<std::optional<Pose>, 2>& grip_poses() const { return hands_.grip_poses; }

   private:
    std::optional<Pose> aim(unsigned hand) const;
    std::optional<std::pair<std::size_t, PanelHit>> nearest(unsigned hand) const;
    Hit hit(const ScenePanel& panel, PanelHit point) const;
    std::vector<ScenePanel> panels_;
    ControllerState hands_;
    std::array<std::optional<Pose>, 2> anchors_;
    std::array<std::optional<PanelKind>, 2> anchored_kinds_;
};
ui::Canvas draw_panel(const PanelSnapshot& app, const ScenePanel& panel, double now);
}  // namespace lumaframevr::application
