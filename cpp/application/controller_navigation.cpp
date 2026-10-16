#include "application_controller.hpp"
#include <algorithm>
#include <cmath>
namespace lumaframevr::application {
namespace {
bool same_cursor(const std::optional<Hit>& a, const std::optional<Hit>& b) {
    if(!a || !b)
        return a.has_value() == b.has_value();
    return a->panel == b->panel && a->width == b->width && a->height == b->height && a->x == b->x && a->y == b->y && a->slot == b->slot;
}
}  // namespace
bool Controller::interacted(const ControllerState& controls) const {
    for(bool value : controls.held)
        if(value)
            return true;
    for(bool value : controls.pressed)
        if(value)
            return true;
    for(bool value : controls.grips)
        if(value)
            return true;
    for(float value : controls.triggers)
        if(value > 0)
            return true;
    for(const auto& stick : controls.sticks)
        if(std::abs(stick[0]) > .2F || std::abs(stick[1]) > .2F)
            return true;
    return false;
}
void Controller::update(const ControllerState& controls, const std::array<std::optional<Hit>, 2>& hits, double now) {
    if(!startup_started_)
        startup_started_ = now;
    if(updater_.poll()) {
        changed = true;
        if(updater_.state().status == ui::UpdateStatus::installed)
            request_exit();
    }
    changed |= explorer.browser.poll();
    start_pending();
    if(images_.update()) {
        if(!images_.error().empty())
            fail(i18n::tr("error.explorer_image", {{"detail", images_.error()}}));
        changed = true;
    }
    changed |= !same_cursor(hover[0], hits[0]) || !same_cursor(hover[1], hits[1]);
    hover = hits;
    changed |= grips_ != controls.grips;
    grips_ = controls.grips;
    const bool activity = interacted(controls);
    const bool bar_hover = std::any_of(hits.begin(), hits.end(), [](const auto& hit) { return hit && hit->panel == PanelKind::transport; });
    if(playing() && (activity || bar_hover) && !minimized_)
        visible_until_ = now + 10;
    if(!activity)
        minimized_ = false;
    if(volume_open && !transport_visible(now)) {
        volume_open = false;
        changed = true;
    }
    navigate(controls, now);
    if(language_input(controls))
        return;
    if(controls.pressed[2]) {
        if(viewer_settings_open_)
            viewer_settings_open_ = false;
        else if(volume_open)
            volume_open = false;
        else if(dialogs.shown(ui::AdvancedKind::settings) || dialogs.shown(ui::AdvancedKind::rotation))
            dialogs.close();
        else if(playing())
            stop();
        else if(explorer.browser.can_back())
            explorer.browser.back();
        else
            explorer.browser.parent();
        changed = true;
    } else if(controls.pressed[0] && playing()) {
        dialogs.toggle(ui::AdvancedKind::rotation);
        changed = true;
    } else if(controls.pressed[3] && playing()) {
        dialogs.toggle(ui::AdvancedKind::settings);
        changed = true;
    } else if(controls.pressed[1]) {
        const auto found = std::find_if(hits.begin(), hits.end(), [](const auto& hit) { return hit.has_value(); });
        if(found != hits.end())
            activate(**found, now);
        else if(viewer_settings_open_) {
            const auto state = viewer_settings();
            const std::array flags{state.show_hidden, state.show_file_sizes, state.save_thumbnails, state.remember_last_state};
            if(viewer_setting_selected_ == ui::viewer_setting_update_row)
                start_viewer_update();
            else
                set_viewer_setting(viewer_setting_selected_, !flags[viewer_setting_selected_]);
        } else if(!playing())
            select_entry(explorer.browser.selected());
        else if(dialogs.shown(ui::AdvancedKind::settings))
            advanced_->navigate(0, 1);
        changed = true;
    }
    persist_viewer();
    if(changed)
        ++revision;
}
void Controller::navigate(const ControllerState& controls, double now) {
    const bool rotation = playing() && dialogs.shown(ui::AdvancedKind::rotation);
    const bool settings = playing() && dialogs.shown(ui::AdvancedKind::settings);
    const int button_x = static_cast<int>(controls.held[7]) - static_cast<int>(controls.held[6]);
    const int button_y = static_cast<int>(controls.held[5]) - static_cast<int>(controls.held[4]);
    const auto& stick = controls.sticks[1];
    const int stick_x = std::abs(stick[0]) > .6F ? (stick[0] > 0 ? 1 : -1) : 0;
    const int stick_y = std::abs(stick[1]) > .6F ? (stick[1] > 0 ? -1 : 1) : 0;
    if(rotation)
        changed |= advanced_->adjust_sticks(controls.sticks, now, 1);
    else
        advanced_->reset_stick_clock();
    const std::array<int, 2> direction{button_x != 0 ? button_x : stick_x, button_y != 0 ? button_y : stick_y};
    const bool repeat = direction != last_direction_ || now >= next_navigation_;
    if(repeat && (direction[0] || direction[1])) {
        if(viewer_settings_open_) {
            viewer_setting_selected_ = static_cast<std::size_t>(std::clamp(static_cast<int>(viewer_setting_selected_) + direction[1], 0, static_cast<int>(ui::viewer_setting_update_row)));
            if(direction[0] && viewer_setting_selected_ == ui::viewer_setting_update_row)
                ++update_page_;
            else if(direction[0])
                set_viewer_setting(viewer_setting_selected_, direction[0] > 0);
            changed = true;
        } else if(settings) {
            advanced_->navigate(direction[1], direction[0]);
            changed = true;
        } else if(!rotation && (!playing() || grips_[0] || grips_[1])) {
            const int columns =
                    std::max(1, ui::browser_viewport(ui::browser_listing_width, playing() ? 368 : 800, explorer.browser.entries().size(), explorer.browser.selected(), explorer.view).columns);
            explorer.browser.move_selection(direction[1] * columns + (explorer.view == ui::BrowserMode::Grid ? direction[0] : 0));
            if(explorer.view == ui::BrowserMode::List) {
                if(direction[0] < 0)
                    explorer.browser.parent();
                if(direction[0] > 0)
                    select_entry(explorer.browser.selected());
            }
            changed = true;
        }
        next_navigation_ = now + (direction != last_direction_ ? .35 : .08);
    }
    last_direction_ = direction;
    const bool seeking = playing() && !viewer_settings_open_ && !viewing_image() && !rotation && !settings && !grips_[0] && !grips_[1] && !controls.pressed[2];
    const int seconds = seek_repeat_[0].update(seeking ? button_x : 0, now);
    if(seconds && playback_.current())
        sought_from([&] { return playback_.current()->skip(seconds); }, now, true);
}
}  // namespace lumaframevr::application
