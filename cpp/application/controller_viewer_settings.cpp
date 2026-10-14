#include "application_controller.hpp"
#include <algorithm>
#include <iostream>
namespace lumaframevr::application {
ui::ViewerSettingsState Controller::viewer_settings() const {
    // Read once: the licences are on disk beside the player and never change while it runs.
    static const std::vector<std::string> licences = [] {
        std::vector<std::string> names;
        const char* root = std::getenv("LUMAFRAMEVR_PACKAGE_ROOT");
        std::error_code error;
        const auto directory = (root ? std::filesystem::path(root) : std::filesystem::path{}) / "licenses";
        for(const auto& entry : std::filesystem::directory_iterator(directory, error))
            if(entry.is_regular_file())
                names.push_back(entry.path().filename().string());
        std::sort(names.begin(), names.end());
        return names;
    }();
    ui::ViewerSettingsState state{viewer_settings_open_,
                                  explorer.browser.hidden(),
                                  viewer_preferences_.save_thumbnails,
                                  viewer_preferences_.remember_last_state,
                                  viewer_setting_selected_,
                                  i18n::active().active(),
                                  viewer_store_.error().empty() ? explorer.tiles.persistence_error() : viewer_store_.error()};
    state.licences = licences;
    state.update = updater_.state();
    if(state.update.status == ui::UpdateStatus::up_to_date)
        state.update.version = LUMAFRAMEVR_VERSION;
    state.update_page = update_page_;
    return state;
}
void Controller::begin_first_start() {
    language_prompt_ = viewer_state_path_.has_value() && !i18n::active().languages().empty() && !viewer_store_.existing() && !playing();
    if(language_prompt_)
        changed = true;
}
void Controller::set_viewer_language(std::string_view code) {
    if(code.empty() || code == i18n::active().active() || !i18n::active().select(code))
        return;
    viewer_preferences_.language = code;
    changed = true;
    ++revision;
    persist_viewer();
}
void Controller::confirm_language() {
    const auto& languages = i18n::active().languages();
    if(!languages.empty())
        set_viewer_language(languages[language_selected_ % languages.size()].code);
    language_prompt_ = false;
    changed = true;
    ++revision;
}
bool Controller::language_input(const ControllerState& controls) {
    if(!language_prompt_)
        return false;
    const auto count = std::max<std::size_t>(1, i18n::active().languages().size());
    const int direction = (controls.held[7] ? 1 : 0) - (controls.held[6] ? 1 : 0);
    if(direction != language_direction_) {
        language_selected_ = static_cast<std::size_t>((static_cast<int>(language_selected_) + direction + static_cast<int>(count)) % static_cast<int>(count));
        language_direction_ = direction;
        changed = true;
    }
    if(controls.pressed[1])
        confirm_language();
    else if(controls.pressed[2]) {
        language_prompt_ = false;
        changed = true;
        ++revision;
    }
    persist_viewer();
    return true;
}
void Controller::initialize_viewer(const ApplicationOptions& options) {
    i18n::active().load(i18n::default_directory());
    viewer_state_path_ = options.viewer_state;
    viewer_preferences_ = viewer_store_.load();
    if(!i18n::active().select(viewer_preferences_.language) && i18n::active().covers("en"))
        i18n::active().select("en");
    const auto& languages = i18n::active().languages();
    for(std::size_t index = 0; index < languages.size(); ++index)
        if(languages[index].code == i18n::active().active())
            language_selected_ = index;
    if(viewer_preferences_.show_hidden != explorer.browser.hidden())
        explorer.browser.toggle_hidden();
    explorer.view = viewer_preferences_.grid ? ui::BrowserMode::Grid : ui::BrowserMode::List;
    if(viewer_preferences_.descending != explorer.browser.descending())
        explorer.browser.toggle_sort();
    if(viewer_preferences_.remember_last_state) {
        volume_ = viewer_preferences_.volume;
        muted_ = viewer_preferences_.muted;
    }
    if(viewer_state_path_ && viewer_preferences_.save_thumbnails)
        explorer.tiles.persistence(viewer_thumbnail_path(*viewer_state_path_));
    if(!viewer_store_.error().empty())
        std::cerr << "Viewer state unavailable: " << viewer_store_.error() << '\n';
}
void Controller::persist_viewer() {
    viewer_preferences_.show_hidden = explorer.browser.hidden();
    viewer_preferences_.folder = explorer.browser.directory();
    viewer_preferences_.volume = volume_;
    viewer_preferences_.muted = muted_;
    viewer_preferences_.grid = explorer.view == ui::BrowserMode::Grid;
    viewer_preferences_.descending = explorer.browser.descending();
    viewer_store_.save(viewer_preferences_);
}
void Controller::set_viewer_setting(std::size_t index, bool enabled) {
    if(index == 0 && enabled != explorer.browser.hidden())
        explorer.browser.toggle_hidden();
    else if(index == 1 && viewer_preferences_.save_thumbnails != enabled) {
        viewer_preferences_.save_thumbnails = enabled;
        explorer.tiles.persistence(enabled && viewer_state_path_ ? std::optional(viewer_thumbnail_path(*viewer_state_path_)) : std::nullopt);
        explorer.tiles.reset();
    } else if(index == 2)
        viewer_preferences_.remember_last_state = enabled;
    if(index >= 3)
        return;
    viewer_setting_selected_ = index;
    changed = true;
    ++revision;
    persist_viewer();
}
void Controller::click_language_prompt(const Hit& hit) {
    const auto& languages = i18n::active().languages();
    if(hit.panel == PanelKind::browser) {
        if(const auto target = ui::language_prompt_at(languages, std::min(hit.width, ui::browser_listing_width), hit.height, hit.x, hit.y)) {
            for(std::size_t index = 0; index < languages.size(); ++index)
                if(languages[index].code == target->code)
                    language_selected_ = index;
            confirm_language();
            return;
        }
    }
    changed = true;
    ++revision;
}
void Controller::start_viewer_update() {
    const char* root = std::getenv("LUMAFRAMEVR_PACKAGE_ROOT");
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    const auto directory = root ? std::filesystem::path(root) : executable.parent_path();
    updater_.start(directory / "update.sh", updater_.state().status == ui::UpdateStatus::available);
    update_page_ = 0;
    viewer_setting_selected_ = 3;
    changed = true;
    ++revision;
}
void Controller::click_viewer_setting(const Hit& hit) {
    if(const auto target = ui::viewer_setting_at(std::min(hit.width, ui::browser_listing_width), hit.height, i18n::active().languages(), hit.x, hit.y, viewer_settings())) {
        if(target->index == 3)
            viewer_settings_open_ = false;
        else if(target->index == 4)
            set_viewer_language(target->language);
        else if(target->index == 5)
            start_viewer_update();
        else if(target->index == 6)
            ++update_page_;
        else
            set_viewer_setting(target->index, target->value);
    } else if(const auto target = ui::browser_target_at(explorer.browser, std::min(hit.width, ui::browser_listing_width), hit.height, {}, hit.x, hit.y);
              target && target->action == ui::BrowserAction::Settings)
        viewer_settings_open_ = false;
    changed = true;
    ++revision;
}
}  // namespace lumaframevr::application
