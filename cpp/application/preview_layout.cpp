#include "preview_layout.hpp"
#include "advanced_dialog_state.hpp"
#include "thumbnail_services.hpp"
namespace lumaframevr::application {
PreviewLayout preview_layout(PreviewInputs input, std::optional<PreviewPanel> panel) {
    PreviewLayout result;
    result.clear = input.explorer != input.previous_explorer || (input.explorer && input.selection_changed) || (!input.explorer && !input.transport_visible);
    result.visible = input.explorer || input.transport_visible;
    if(!panel || !input.shown || !result.visible)
        return result;
    if(panel->kind != (input.explorer ? PanelKind::browser : PanelKind::transport))
        return result;
    PreviewPlacement placement;
    if(input.explorer) {
        placement.width = panel->size[0] * 288 / panel->pixels[0];
        placement.offset.position = {(1360.0F / panel->pixels[0] - .5F) * panel->size[0], (.5F - 145.0F / panel->pixels[1]) * panel->size[1], .004F};
    } else {
        placement.width = ui::advanced_reference_width * preview_extent[0] / 792;
        placement.offset.position = {SeekPreviewReadout::center(input.position, input.duration) * (.6F / 1.4F), ui::advanced_reference_width * (preview_extent[1] + 400) / (2 * 792) + .02F, .004F};
    }
    placement.height = placement.width * preview_extent[1] / preview_extent[0];
    result.placement = placement;
    return result;
}
}  // namespace lumaframevr::application
