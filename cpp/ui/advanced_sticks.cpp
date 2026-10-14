#include "advanced_panels.hpp"
#include <algorithm>
#include <cmath>
namespace lumaframevr::ui {
bool AdvancedSettingsController::adjust_sticks(const std::array<std::array<float, 2>, 2>& sticks, double now, float vertical_sign) {
    if(!std::isfinite(now))
        throw std::invalid_argument("Nonfinite dialog stick time");
    const auto seconds = stick_at_ ? static_cast<float>(std::clamp(now - *stick_at_, 0.0, 0.1)) : 0.0F;
    stick_at_ = now;
    if(seconds == 0)
        return false;
    const auto before = presentation_;
    presentation_.alignment.adjust(mode_, sticks, seconds, presentation_.stereo, vertical_sign);
    remember_if_changed(before);
    return before != presentation_;
}
}  // namespace lumaframevr::ui
