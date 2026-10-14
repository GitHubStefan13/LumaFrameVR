#include "application_controller.hpp"
namespace lumaframevr::application {
SceneSnapshot Controller::scene_snapshot(double now) const {
    SceneSnapshot result{panels(), playing(), transport_visible(now), startup_visible(now), telemetry.rows(0, 0, 0).size(), bar_offset, {}};
    for(unsigned index = 0; index < 2; ++index) {
        const auto kind = static_cast<ui::AdvancedKind>(index);
        if(dialogs.shown(kind))
            result.dialogs[index] = dialogs.offset(kind);
    }
    return result;
}
PanelSnapshot Controller::panel_snapshot() const {
    return {explorer.browser,
            explorer.view,
            explorer.tiles.shown(explorer.visible()),
            explorer.preview_details,
            retiring(),
            passthrough_enabled,
            passthrough_supported,
            hover,
            error,
            transport(),
            presentation,
            advanced_->selected(),
            advanced_->mode(),
            path,
            telemetry,
            device_sample,
            xr_fps,
            target_fps,
            viewer_settings(),
            {language_prompt_, language_selected_}};
}
}  // namespace lumaframevr::application
