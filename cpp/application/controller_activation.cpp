#include "application_controller.hpp"
#include <algorithm>
#include <cmath>
namespace lumaframevr::application {
void Controller::activate(const Hit& hit, double now) {
    if(language_prompt_) {
        click_language_prompt(hit);
        return;
    }
    if(hit.panel == PanelKind::browser && viewer_settings_open_) {
        click_viewer_setting(hit);
        return;
    }
    if(hit.panel == PanelKind::browser) {
        ui::BrowserRenderState state;
        state.mode = explorer.view;
        state.passthrough_enabled = passthrough_enabled;
        state.passthrough_supported = passthrough_supported;
        if(const auto target = ui::browser_target_at(explorer.browser, std::min(hit.width, ui::browser_listing_width), hit.height, state, hit.x, hit.y))
            browser_action(*target);
        else if(const auto entry =
                        ui::browser_entry_at(std::min(hit.width, ui::browser_listing_width), hit.height, explorer.browser.entries().size(), explorer.browser.selected(), explorer.view, hit.x, hit.y))
            select_entry(*entry);
    } else if(hit.panel == PanelKind::settings)
        advanced_->click_setting(hit.x, hit.y);
    else if(hit.panel == PanelKind::rotation)
        advanced_->click_alignment(hit.x, hit.y);
    else if(hit.panel == PanelKind::transport) {
        const auto target = ui::transport_hit(hit.x, hit.y, transport().duration, volume_open, viewing_image());
        if(!target)
            return;
        using A = ui::TransportAction;
        switch(target->action) {
            case A::Button:
                transport_button(target->button, now);
                break;
            case A::Seek:
                seek(target->value, now);
                break;
            case A::Volume:
                if(std::isfinite(target->value))
                    volume_ = std::clamp(static_cast<float>(target->value), 0.0F, 1.0F);
                volume_pending_ = true;
                apply_volume();
                break;
            case A::VolumeToggle:
                volume_open = !volume_open;
                break;
            case A::Mute:
                muted_ = !muted_;
                volume_pending_ = true;
                apply_volume();
                break;
            case A::Move:
                break;
            case A::Passthrough:
                toggle_passthrough();
                break;
            case A::Minimize:
                volume_open = false;
                minimized_ = true;
                visible_until_ = 0;
                seek_repeat_ = {};
                dialogs.close();
                break;
        }
    }
    changed = true;
    ++revision;
}
}  // namespace lumaframevr::application
