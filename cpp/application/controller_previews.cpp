#include "application_controller.hpp"
namespace lumaframevr::application {
TileWanted Controller::wanted_tiles() const {
    return explorer.wanted(playing());
}
void Controller::update_previews() {
    const auto update = explorer.update(playing(), list_drag_.has_value());
    if(update.error)
        error = *update.error;
    if(update.changed) {
        changed = true;
        ++revision;
    }
}
}  // namespace lumaframevr::application
