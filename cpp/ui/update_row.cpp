#include "viewer_settings.hpp"
#include "font_style.hpp"
#include <algorithm>

namespace lumaframevr::ui {
namespace {
std::vector<std::string> fitting_lines(std::string_view text, int width, FontStyle style) {
    std::vector<std::string> lines;
    std::u32string line;
    for(const auto character : decode_utf8(text)) {
        auto candidate = line + character;
        if(!line.empty() && text_width(encode_utf8(candidate), style) > width) {
            lines.push_back(encode_utf8(line));
            line.clear();
        }
        line += character;
    }
    if(!line.empty())
        lines.push_back(encode_utf8(line));
    return lines;
}
std::string update_text(const UpdateState& state) {
    if(state.status == UpdateStatus::up_to_date)
        return i18n::tr("viewer.update.latest", {{"version", state.version}});
    if(state.status == UpdateStatus::available)
        return i18n::tr("viewer.update.available", {{"version", state.version}}) + " " + state.message;
    return state.message;
}
Rect measured_button(int x, int y, int width, std::string_view label, std::vector<std::string>& lines) {
    const auto measured = std::min(width, control_width(label, FontStyle::medium, false, 24));
    lines = fitting_lines(label, measured - 24, FontStyle::medium);
    return {x, y, measured, std::max(34, static_cast<int>(lines.size()) * text_height(FontStyle::medium) + 18)};
}
}  // namespace
UpdateRowLayout update_row_layout(Rect area, const UpdateState& state, std::size_t page) {
    UpdateRowLayout result;
    const auto text = update_text(state);
    const auto lines = fitting_lines(text, area.width, FontStyle::secondary);
    constexpr std::size_t page_lines = 6;
    result.pages = std::max<std::size_t>(1, (lines.size() + page_lines - 1) / page_lines);
    const auto start = (page % result.pages) * page_lines;
    for(std::size_t index = start; index < std::min(start + page_lines, lines.size()); ++index) result.lines.push_back(lines[index]);
    const auto label = i18n::tr(state.status == UpdateStatus::available ? "viewer.update.install" : "viewer.update.check");
    const auto more_label = i18n::tr("viewer.update.more", {{"page", std::to_string(page % result.pages + 1)}, {"pages", std::to_string(result.pages)}});
    const auto more_width = result.pages > 1 ? std::min(area.width / 2, control_width(more_label, FontStyle::medium, false, 24)) : 0;
    result.button = measured_button(area.x, area.y, area.width - (more_width ? more_width + 8 : 0), label, result.button_lines);
    if(more_width)
        result.more = measured_button(area.right() - more_width, area.y, more_width, more_label, result.more_lines);
    area.height = std::max(result.button.height, result.more.height) + 8 + static_cast<int>(result.lines.size()) * text_height(FontStyle::secondary) + 8;
    result.area = area;
    return result;
}
}  // namespace lumaframevr::ui
