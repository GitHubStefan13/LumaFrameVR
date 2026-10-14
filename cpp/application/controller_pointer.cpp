#include "application_controller.hpp"
#include <algorithm>
namespace lumaframevr::application {
std::optional<PanelKind> Controller::grabbed_panel(unsigned hand) const {
    if(panel_drag_ && panel_drag_->hand == hand)
        return panel_drag_->start.panel;
    return {};
}
void Controller::pointer(unsigned hand, std::optional<Hit> hit, bool held, double now) {
    if(hand > 1)
        return;
    const bool pressed = held && !triggers_[hand];
    triggers_[hand] = held;
    if(list_drag_ && list_drag_->hand == hand) {
        if(!hit || hit->panel != PanelKind::browser)
            list_drag_->scrolled = true;
        if(!held) {
            const auto drag = *list_drag_;
            list_drag_.reset();
            if(!drag.scrolled && hit && hit->panel == PanelKind::browser)
                select_entry(drag.entry);
        } else if(hit && hit->panel == PanelKind::browser)
            drag_list(*hit);
        return;
    }
    if(panel_drag_ && panel_drag_->hand == hand) {
        const auto kind = panel_drag_->start.panel;
        const auto advanced_kind = kind == PanelKind::settings ? ui::AdvancedKind::settings : ui::AdvancedKind::rotation;
        if(!held) {
            if(kind != PanelKind::transport)
                dialogs.release(advanced_kind, hand);
            panel_drag_.reset();
        } else if(hit && hit->panel == kind) {
            if(kind == PanelKind::transport) {
                const auto& point = panel_drag_->start.point;
                const float width = ui::advanced_reference_width * 1160 / 792;
                bar_offset[0] = std::clamp(bar_offset[0] + (hit->point[0] - point[0]) * width / 2, -1.2F, 1.2F);
                bar_offset[1] = std::clamp(bar_offset[1] + (hit->point[1] - point[1]) * width * 400 / 1160 / 2, -.6F, .6F);
            } else
                dialogs.drag(advanced_kind, hand, hit->point);
            changed = true;
            ++revision;
        }
        return;
    }
    if(settings_drag_ && settings_drag_->hand == hand) {
        if(!held || !hit || hit->panel != PanelKind::settings)
            settings_drag_.reset();
        else if(advanced_->drag_setting(settings_drag_->field, settings_drag_->slider, hit->x)) {
            changed = true;
            ++revision;
        } else
            settings_drag_.reset();
        return;
    }
    if(alignment_drag_ && alignment_drag_->first == hand) {
        if(!held || !hit || hit->panel != PanelKind::rotation)
            alignment_drag_.reset();
        else {
            advanced_->drag_alignment(alignment_drag_->second, hit->x);
            changed = true;
            ++revision;
        }
        return;
    }
    if(!hit) {
        if(pressed && volume_open) {
            volume_open = false;
            changed = true;
        }
        return;
    }
    if(hit->panel == PanelKind::browser && viewer_settings_open_) {
        if(pressed)
            activate(*hit, now);
        return;
    }
    if(pressed && hit->panel == PanelKind::browser) {
        const auto entry =
                ui::browser_entry_at(std::min(hit->width, ui::browser_listing_width), hit->height, explorer.browser.entries().size(), explorer.browser.selected(), explorer.view, hit->x, hit->y);
        if(entry && !list_drag_) {
            list_drag_ = Drag{hand, *hit, hit->y, 0, *entry, false};
            return;
        }
    }
    if(pressed && (hit->panel == PanelKind::settings || hit->panel == PanelKind::rotation) && ui::advanced_corner_dot().contains(hit->x, hit->y)) {
        const auto kind = hit->panel == PanelKind::settings ? ui::AdvancedKind::settings : ui::AdvancedKind::rotation;
        if(!panel_drag_ && dialogs.grab(kind, hand, hit->point))
            panel_drag_ = Drag{hand, *hit, 0, 0, 0, false};
        return;
    }
    bool dragging = false;
    if(hit->panel == PanelKind::transport) {
        const auto target = ui::transport_hit(hit->x, hit->y, transport().duration, volume_open, viewing_image());
        if(pressed && target && target->action == ui::TransportAction::Move && !panel_drag_) {
            panel_drag_ = Drag{hand, *hit, 0, 0, 0, false};
            return;
        }
        dragging = held && target && target->action == ui::TransportAction::Volume;
    }
    if(pressed && hit->panel == PanelKind::settings) {
        const auto fields = ui::settings_fields(presentation);
        for(const auto& row : ui::settings_layout(presentation, advanced_->selected(), hit->width, hit->height))
            if(row.slider && row.slider->contains(hit->x, hit->y))
                settings_drag_ = SettingsDrag{hand, fields[row.index], *row.slider};
    }
    if(pressed && hit->panel == PanelKind::rotation) {
        for(std::size_t row = 0; row < 4; ++row)
            if(ui::advanced_row(row).contains(hit->x, hit->y))
                alignment_drag_ = std::pair{hand, row};
    }
    if(pressed || dragging)
        activate(*hit, now);
}
}  // namespace lumaframevr::application
