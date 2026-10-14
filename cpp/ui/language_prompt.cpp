#include "language_prompt.hpp"
#include "font_style.hpp"
#include <string>
#include <algorithm>

namespace lumaframevr::ui {
namespace {
constexpr int button_height = 34;
constexpr int gap = 8;
int button_width(std::string_view name) {
    return std::max(64, text_width(name, FontStyle::medium) + 24);
}
}  // namespace

std::vector<LanguageButton> language_buttons(const std::vector<i18n::Language>& languages, Rect area) {
    std::vector<LanguageButton> result;
    int x = area.x;
    int y = area.y;
    for(const auto& language : languages) {
        const auto width = button_width(language.name);
        if(x != area.x && x + width > area.right()) {
            x = area.x;
            y += button_height + gap;
        }
        result.push_back({{x, y, width, button_height}, language.code, language.name});
        x += width + gap;
    }
    return result;
}

Rect language_prompt_card(int width, int height) {
    return {(width - 780) / 2, (height - 300) / 2, 780, 300};
}

std::optional<LanguageButton> language_prompt_at(const std::vector<i18n::Language>& languages, int width, int height, int x, int y) {
    const auto card = language_prompt_card(width, height);
    for(const auto& button : language_buttons(languages, {card.x + 24, card.y + 96, card.width - 48, button_height}))
        if(button.area.contains(x, y))
            return button;
    return {};
}

void draw_language_prompt(Canvas& canvas, const LanguagePromptState& state, const std::vector<i18n::Language>& languages) {
    const auto card = language_prompt_card(canvas.width(), canvas.height());
    canvas.fill({0, 0, canvas.width(), canvas.height()}, {0, 0, 0, 150});
    canvas.rounded(card, 10, {42, 42, 42, 255});
    draw_text(canvas, card.x + 24, card.y + 24, i18n::tr("welcome.title"), {235, 235, 235, 255}, FontStyle::title);
    draw_text(canvas, card.x + 24, card.y + 52, i18n::tr("welcome.detail"), {156, 156, 156, 255}, FontStyle::secondary);
    const auto buttons = language_buttons(languages, {card.x + 24, card.y + 96, card.width - 48, button_height});
    for(std::size_t index = 0; index < buttons.size(); ++index) {
        const bool active = index == (languages.empty() ? 0 : state.selected % languages.size());
        canvas.rounded(buttons[index].area, 5, active ? Color{54, 106, 71, 255} : Color{65, 65, 65, 255});
        draw_text(canvas, buttons[index].area.x + 12, buttons[index].area.y + 9, buttons[index].name, {230, 230, 230, 255}, FontStyle::medium);
    }
    draw_text(canvas, card.x + 24, card.y + 254, i18n::tr("welcome.hint"), {160, 160, 160, 255}, FontStyle::secondary);
    // The first thing a new installation says: what this is, and that it is not for sale.
    const auto notice = wrap_text(i18n::tr("welcome.free"), card.width - 48, FontStyle::secondary);
    for(std::size_t index = 0; index < notice.size(); ++index) draw_text(canvas, card.x + 24, card.y + 176 + static_cast<int>(index) * 20, notice[index], {190, 190, 190, 255}, FontStyle::secondary);
}
}  // namespace lumaframevr::ui
