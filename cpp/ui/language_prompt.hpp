#pragma once
#include "canvas.hpp"
#include "catalogue.hpp"
#include <optional>
#include <string>
#include <vector>

namespace lumaframevr::ui {
struct LanguageButton {
    Rect area;
    std::string code;
    std::string name;
};
struct LanguagePromptState {
    bool open{};
    std::size_t selected{};
};
std::vector<LanguageButton> language_buttons(const std::vector<i18n::Language>& languages, Rect area);
Rect language_prompt_card(int width, int height);
std::optional<LanguageButton> language_prompt_at(const std::vector<i18n::Language>& languages, int width, int height, int x, int y);
void draw_language_prompt(Canvas& canvas, const LanguagePromptState& state, const std::vector<i18n::Language>& languages);
}  // namespace lumaframevr::ui
