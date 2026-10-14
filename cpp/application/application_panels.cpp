#include "application_panels.hpp"
#include "video_surface.hpp"
namespace lumaframevr::application {
std::vector<XrCompositionLayerQuad> Panels::update(Controller& app, const Scene& scene, double now, std::optional<std::filesystem::path>& snapshot) {
    std::vector<XrCompositionLayerQuad> output;
    for(const auto& panel : scene.panels()) {
        auto& slot = slots_.at(panel.slot);
        bool force = false;
        if(!slot.panel || slot.pixels != panel.pixels) {
            slot.panel = std::make_unique<OpenXrPanel>(session_, panel.pixels[0], panel.pixels[1]);
            slot.pixels = panel.pixels;
            force = true;
        }
        if(force || slot.kind != panel.kind || slot.revision != app.revision || app.changed || now - slot.painted >= .2 || snapshot) {
            auto canvas = draw_panel(app.panel_snapshot(), panel, now);
            slot.panel->upload(canvas);
            slot.kind = panel.kind;
            slot.revision = app.revision;
            slot.painted = now;
            if(snapshot) {
                if(!snapshot->parent_path().empty())
                    std::filesystem::create_directories(snapshot->parent_path());
                canvas.save_ppm(*snapshot);
                snapshot.reset();
            }
        }
        const float reference = panel.surface.size[0] * 792 / panel.pixels[0];
        output.push_back(slot.panel->layer(session_.local_space(), xr_pose(panel.surface.pose), reference));
    }
    for(unsigned index = 0; index < slots_.size(); ++index) {
        bool visible = false;
        for(const auto& panel : scene.panels()) visible |= panel.slot == index;
        if(!visible)
            slots_[index].kind.reset();
    }
    app.changed = false;
    return output;
}
}  // namespace lumaframevr::application
