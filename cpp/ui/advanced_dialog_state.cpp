#include "advanced_dialog_state.hpp"
#include <algorithm>
namespace lumaframevr::ui {
Pose advanced_pose(AdvancedKind, std::array<float, 2> bar, std::array<float, 2> offset) {
    Pose pose;
    pose.position[0] = bar[0] + advanced_reference_width * (1160.0F + 792.0F) / 792.0F / 2 + 0.06F + offset[0];
    pose.position[1] = -(0.62F / 1.75F) * 0.6F + bar[1] + offset[1];
    pose.position[2] = -2.6F;
    return pose;
}
void AdvancedDialogs::open(AdvancedKind kind) {
    for(std::size_t i = 0; i < dialogs_.size(); ++i) dialogs_[i].open = i == static_cast<std::size_t>(kind);
}
void AdvancedDialogs::toggle(AdvancedKind kind) {
    const bool value = !shown(kind);
    close();
    if(value)
        open(kind);
}
void AdvancedDialogs::close() {
    for(auto& dialog : dialogs_) {
        dialog.open = false;
        dialog.grab.reset();
    }
}
bool AdvancedDialogs::shown(AdvancedKind kind) const {
    return dialogs_.at(static_cast<std::size_t>(kind)).open;
}
bool AdvancedDialogs::grab(AdvancedKind kind, unsigned hand, std::array<float, 2> point) {
    auto& dialog = dialogs_.at(static_cast<std::size_t>(kind));
    if(dialog.grab || hand > 1 || !dialog.open)
        return false;
    dialog.grab = std::pair{hand, point};
    return true;
}
bool AdvancedDialogs::drag(AdvancedKind kind, unsigned hand, std::array<float, 2> point) {
    auto& dialog = dialogs_.at(static_cast<std::size_t>(kind));
    if(!dialog.grab || dialog.grab->first != hand)
        return false;
    const auto height = kind == AdvancedKind::settings ? settings_size[1] : alignment_size[1];
    dialog.offset[0] = std::clamp(dialog.offset[0] + (point[0] - dialog.grab->second[0]) * advanced_reference_width / 2, -1.2F, 1.2F);
    dialog.offset[1] = std::clamp(dialog.offset[1] + (point[1] - dialog.grab->second[1]) * advanced_reference_width * height / 792 / 2, -0.6F, 0.6F);
    return true;
}
void AdvancedDialogs::release(AdvancedKind kind, unsigned hand) {
    auto& dialog = dialogs_.at(static_cast<std::size_t>(kind));
    if(dialog.grab && dialog.grab->first == hand)
        dialog.grab.reset();
}
std::array<float, 2> AdvancedDialogs::offset(AdvancedKind kind) const {
    return dialogs_.at(static_cast<std::size_t>(kind)).offset;
}
}  // namespace lumaframevr::ui
