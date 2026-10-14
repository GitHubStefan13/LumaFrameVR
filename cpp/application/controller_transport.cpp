#include "application_controller.hpp"
#include <algorithm>
#include <cmath>
namespace lumaframevr::application {
bool Controller::transport_visible(double now) const {
    return playing() && !minimized_ && (now < visible_until_ || seek_generation_.has_value());
}
std::array<std::optional<PanelKind>, 3> Controller::panels() const {
    if(!playing())
        return {std::nullopt, std::nullopt, PanelKind::browser};
    // A still image has no transport bar, so without a panel the open is invisible: the browser
    // is gone and nothing replaces it until the upload lands, which for a large image is seconds.
    const bool information = !grips_[0] && !grips_[1] && (hud || (viewing_image() && image_loading()));
    return {grips_[0] ? std::optional(PanelKind::browser) : std::nullopt, grips_[1] ? std::optional(PanelKind::browser) : std::nullopt,
            information ? std::optional(PanelKind::information) : std::nullopt};
}
ui::TransportState Controller::transport() const {
    ui::TransportState state;
    state.title = path.filename().string();
    state.position = playback_.current() ? playback_.current()->position() : 0;
    state.duration = playback_.current() ? playback_.current()->duration() : std::nullopt;
    state.volume = volume_;
    state.muted = muted_;
    state.playing = !viewing_image() && !paused_;
    state.still_image = viewing_image();
    state.image_loading = image_loading();
    const auto dimensions = image_dimensions();
    state.image_width = dimensions[0];
    state.image_height = dimensions[1];
    state.volume_open = !viewing_image() && volume_open;
    state.speed = viewing_image() ? 1 : std::max(1U, seek_repeat_[0].speed());
    state.passthrough_enabled = passthrough_enabled;
    state.passthrough_supported = passthrough_supported;
    return state;
}
void Controller::toggle_passthrough() {
    if(!passthrough_supported) {
        passthrough_enabled = false;
        passthrough_error_ = i18n::tr("error.passthrough_unavailable", {{"detail", passthrough_reason.empty() ? i18n::tr("error.passthrough_unsupported") : passthrough_reason}});
        error = passthrough_error_;
    } else {
        passthrough_enabled = !passthrough_enabled;
        if(!passthrough_error_.empty() && error == passthrough_error_)
            error.clear();
        passthrough_error_.clear();
    }
    changed = true;
    ++revision;
}
void Controller::seek(double seconds, double now) {
    if(!playback_.current() || !std::isfinite(seconds) || seconds < 0)
        return;
    sought_from([&] { return playback_.current()->seek(seconds); }, now, true);
}
void Controller::sought(std::optional<std::uint64_t> generation, double now, bool preview) {
    seek_generation_ = generation;
    telemetry.sync.reset_window();
    visible_until_ = now + 10;
    if(preview && preview_cadence_.due(now)) {
        playback_.current()->request_preview();
        ++preview_requests_;
    }
    changed = true;
}
void Controller::transport_button(unsigned button, double now) {
    if(!playback_.current() && !viewing_image())
        return;
    if(viewing_image() && (button == 0 || button == 2)) {
        adjacent_image(button == 0 ? -1 : 1);
        return;
    }
    if(viewing_image() && button == 1) {
        stop();
        return;
    }
    if(button == 1 && playback_.current()) {
        paused_ = !paused_;
        playback_.current()->pause(paused_);
    } else if((button == 0 || button == 2) && playback_.current()) {
        sought_from([&] { return playback_.current()->skip(button == 0 ? -10 : 10); }, now, false);
    } else if(button == 3)
        dialogs.toggle(ui::AdvancedKind::settings);
    else if(button == 4)
        dialogs.toggle(ui::AdvancedKind::rotation);
    else if(button == 5)
        hud = !hud;
    visible_until_ = now + 10;
    changed = true;
}
void Controller::browser_action(const ui::BrowserTarget& target) {
    if(!target.enabled)
        return;
    using A = ui::BrowserAction;
    switch(target.action) {
        case A::Back:
            explorer.browser.back();
            break;
        case A::Forward:
            explorer.browser.forward();
            break;
        case A::Up:
            explorer.browser.parent();
            break;
        case A::Refresh:
            explorer.browser.refresh();
            explorer.tiles.reset();
            break;
        case A::Sort:
            explorer.browser.toggle_sort();
            break;
        case A::Hidden:
            set_viewer_setting(0, !explorer.browser.hidden());
            break;
        case A::Settings:
            viewer_settings_open_ = !viewer_settings_open_;
            break;
        case A::Quit:
            request_exit();
            break;
        case A::Passthrough:
            toggle_passthrough();
            break;
        case A::Place:
            if(target.index < explorer.browser.places().size())
                explorer.browser.go(explorer.browser.places()[target.index].second);
            break;
        case A::Breadcrumb: {
            const auto ancestors = ui::browser_ancestors(explorer.browser.directory());
            if(target.index < ancestors.size())
                explorer.browser.go(ancestors[target.index]);
            break;
        }
        case A::List:
            explorer.view = ui::BrowserMode::List;
            explorer.tiles.reset();
            break;
        case A::Grid:
            explorer.view = ui::BrowserMode::Grid;
            explorer.tiles.reset();
            break;
    }
    list_drag_.reset();
    persist_viewer();
    changed = true;
}
}  // namespace lumaframevr::application
