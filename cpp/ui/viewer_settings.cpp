#include "viewer_settings.hpp"
#include "font_style.hpp"
#include <algorithm>

namespace lumaframevr::ui {
namespace {
// The toggle labels, in the order the panel draws them, one per index below viewer_setting_toggles.
const std::array<std::string_view, viewer_setting_toggles> label_keys{"viewer.hidden", "viewer.file_sizes", "viewer.thumbnails", "viewer.remember"};
const std::array<std::string_view, viewer_setting_toggles> detail_keys{"viewer.hidden.detail", "viewer.file_sizes.detail", "viewer.thumbnails.detail", "viewer.remember.detail"};
constexpr int row_height = 64;
}  // namespace

Rect viewer_settings_card(int width, int height) {
    // Taller than it was: below the toggles and the language row there is now the licence list and
    // the notice that says what this software is and that nobody should charge for it. Four toggles
    // instead of three is another row's worth of height on top of that.
    return {(width - 780) / 2, (height - 740) / 2, 780, 740};
}

std::vector<ViewerSettingTarget> viewer_setting_targets(int width, int height, const std::vector<i18n::Language>& languages, const ViewerSettingsState& state) {
    const auto card = viewer_settings_card(width, height);
    std::vector<ViewerSettingTarget> result;
    for(std::size_t index = 0; index < viewer_setting_toggles; ++index) {
        const auto y = card.y + 62 + static_cast<int>(index) * row_height;
        // Sized from the words they draw: "On" and "Off" are short today and will not be in every
        // language, and a clipped toggle is worse than a wide one.
        const auto off = control_width(i18n::tr("viewer.off"), FontStyle::medium) + 12;
        const auto on = control_width(i18n::tr("viewer.on"), FontStyle::medium) + 12;
        result.push_back({{card.right() - 24 - off - 8 - on, y, on, 34}, index, false, {}});
        result.push_back({{card.right() - 24 - off, y, off, 34}, index, true, {}});
    }
    result.push_back({{card.right() - 24 - control_width(i18n::tr("viewer.close"), FontStyle::medium) - 12, card.y + 12, control_width(i18n::tr("viewer.close"), FontStyle::medium) + 12, 32},
                      viewer_setting_close,
                      false,
                      {}});
    for(const auto& button : language_buttons(languages, {card.x + 24, card.y + 380, card.width - 48, 34})) result.push_back({button.area, viewer_setting_language, false, button.code});
    const auto update = update_row_layout({card.x + 24, card.y + 420, card.width - 48, 0}, state.update, state.update_page);
    if(state.update.status != UpdateStatus::checking)
        result.push_back({update.button, viewer_setting_update, false, {}});
    if(update.pages > 1)
        result.push_back({update.more, viewer_setting_update_more, false, {}});
    return result;
}

std::optional<ViewerSettingTarget> viewer_setting_at(int width, int height, const std::vector<i18n::Language>& languages, int x, int y, const ViewerSettingsState& state) {
    for(const auto& target : viewer_setting_targets(width, height, languages, state))
        if(target.area.contains(x, y))
            return target;
    return {};
}

void draw_viewer_settings(Canvas& canvas, const ViewerSettingsState& state, const std::vector<i18n::Language>& languages, std::string_view version) {
    const auto card = viewer_settings_card(canvas.width(), canvas.height());
    canvas.fill({6, 96, canvas.width() - 12, canvas.height() - 144}, {24, 24, 24, 255});
    canvas.rounded(card, 10, {42, 42, 42, 255});
    draw_text(canvas, card.x + 24, card.y + 22, i18n::tr("viewer.title"), {230, 230, 230, 255}, FontStyle::title);
    const std::array values{state.show_hidden, state.show_file_sizes, state.save_thumbnails, state.remember_last_state};
    for(std::size_t index = 0; index < viewer_setting_toggles; ++index) {
        const auto y = card.y + 62 + static_cast<int>(index) * row_height;
        if(state.selected == index)
            canvas.rounded({card.x + 12, y - 4, card.width - 24, 56}, 5, {49, 49, 49, 255});
        draw_text(canvas, card.x + 24, y + 3, i18n::tr(label_keys[index]), {224, 224, 224, 255}, FontStyle::medium);
        draw_text(canvas, card.x + 24, y + 28, i18n::tr(detail_keys[index]), {156, 156, 156, 255}, FontStyle::secondary);
    }
    // The version lives here now, quietly, where it belongs: this is the panel a user opens
    // to see what they are running.
    if(!version.empty()) {
        const auto text = i18n::tr("viewer.version", {{"version", std::string(version)}});
        const auto x = card.right() - 24 - text_width(text, FontStyle::medium);
        draw_text(canvas, x, card.bottom() - 26, text, {140, 140, 140, 255}, FontStyle::medium);
    }
    // What the licences are, then which ones shipped, then the notice about paying for it. The
    // notice is placed upwards from the bottom of the card and the list is capped to the room left
    // above it, so the two cannot collide however many licences a package carries. Names longer
    // than their column are cut, because the file names are longer than the panel is wide.
    const auto update = update_row_layout({card.x + 24, card.y + 420, card.width - 48, 0}, state.update, state.update_page);
    if(state.selected == viewer_setting_update_row)
        canvas.rounded({card.x + 12, update.area.y - 4, card.width - 24, update.area.height + 4}, 5, {49, 49, 49, 255});
    const auto status_top = update.area.y + std::max(update.button.height, update.more.height) + 8;
    for(std::size_t index = 0; index < update.lines.size(); ++index)
        draw_text(canvas, update.area.x, status_top + static_cast<int>(index) * text_height(FontStyle::secondary), update.lines[index],
                  state.update.status == UpdateStatus::failed ? Color{255, 160, 130, 255} : Color{190, 190, 190, 255}, FontStyle::secondary);
    const auto licences_top = update.area.bottom() + 8;
    draw_text(canvas, card.x + 24, licences_top, i18n::tr("viewer.licences"), {224, 224, 224, 255}, FontStyle::medium);
    const auto notice = wrap_text(i18n::tr("welcome.free"), card.width - 48, FontStyle::secondary);
    const auto notice_top = card.bottom() - 30 - static_cast<int>(notice.size()) * 20;
    const auto column = (card.width - 48) / 2;
    const auto rows_visible = std::max(0, (notice_top - 12 - (licences_top + 28)) / 18);
    for(std::size_t index = 0; index < state.licences.size(); ++index) {
        const auto row = static_cast<int>(index / 2);
        if(row >= rows_visible)
            break;
        auto name = state.licences[index];
        while(!name.empty() && text_width(name, FontStyle::secondary) > column - 16) name.pop_back();
        const auto x = card.x + 24 + static_cast<int>(index % 2) * column;
        draw_text(canvas, x, licences_top + 28 + row * 18, name, {156, 156, 156, 255}, FontStyle::secondary);
    }
    for(std::size_t index = 0; index < notice.size(); ++index) draw_text(canvas, card.x + 24, notice_top + static_cast<int>(index) * 20, notice[index], {190, 190, 190, 255}, FontStyle::secondary);
    const auto language_row = card.y + 326;
    draw_text(canvas, card.x + 24, language_row + 3, i18n::tr("viewer.language"), {224, 224, 224, 255}, FontStyle::medium);
    draw_text(canvas, card.x + 24, language_row + 28, i18n::tr("viewer.language.detail"), {156, 156, 156, 255}, FontStyle::secondary);
    for(const auto& target : viewer_setting_targets(canvas.width(), canvas.height(), languages, state)) {
        bool active = false;
        if(target.index < viewer_setting_toggles)
            active = values[target.index] == target.value;
        else if(target.index == viewer_setting_language)
            active = target.language == state.language;
        canvas.rounded(target.area, 5, active ? Color{54, 106, 71, 255} : Color{65, 65, 65, 255});
        if(target.index < viewer_setting_toggles) {
            draw_text(canvas, target.area.x + 12, target.area.y + 9, i18n::tr(target.value ? "viewer.on" : "viewer.off"), {230, 230, 230, 255}, FontStyle::medium);
        } else if(target.index == viewer_setting_close) {
            draw_text(canvas, target.area.x + 12, target.area.y + 9, i18n::tr("viewer.close"), {230, 230, 230, 255}, FontStyle::medium);
        } else if(target.index >= viewer_setting_update) {
            const auto& labels = target.index == viewer_setting_update ? update.button_lines : update.more_lines;
            for(std::size_t line = 0; line < labels.size(); ++line)
                draw_text(canvas, target.area.x + 12, target.area.y + 9 + static_cast<int>(line) * text_height(FontStyle::medium), labels[line], {230, 230, 230, 255}, FontStyle::medium);
        } else {
            const auto found = std::find_if(languages.begin(), languages.end(), [&](const auto& language) { return language.code == target.language; });
            draw_text(canvas, target.area.x + 12, target.area.y + 9, found == languages.end() ? target.language : found->name, {230, 230, 230, 255}, FontStyle::medium);
        }
    }
    // The hint, or the error, sits on the bottom line beside the version: the licence list occupies
    // everything between the language row and the notice, so that line is the only free space left.
    draw_text(canvas, card.x + 24, card.bottom() - 26, state.error.empty() ? i18n::tr("viewer.hint") : fit_label(state.error, 59),
              state.error.empty() ? Color{160, 160, 160, 255} : Color{255, 160, 130, 255}, FontStyle::secondary);
}
}  // namespace lumaframevr::ui
