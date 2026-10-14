#include "browser.hpp"
#include "browser_colors.hpp"
#include "catalogue.hpp"
#include "font_style.hpp"
#include <algorithm>

namespace lumaframevr::ui {
void draw_browser(Canvas& canvas, const FolderBrowser& browser, const BrowserRenderState& state) {
    canvas.clear();
    draw_browser_chrome(canvas, browser, state);
    draw_browser_entries(canvas, browser, state);
    // The status bar: how many items, a divider, then the path of whatever is selected.
    using namespace browser_color;
    const auto layout = browser_layout(canvas.width(), canvas.height());
    const auto& directory = browser.directory();
    const auto count = browser.entries().size() - (directory.has_parent_path() && directory.parent_path() != directory ? 1 : 0);
    const auto items = i18n::tr("browser.items", {{"count", std::to_string(count)}});
    const auto baseline = layout.footer.y + (layout.footer.height - text_height(FontStyle::secondary)) / 2;
    draw_text(canvas, layout.footer.x, baseline, items, dim, FontStyle::secondary);
    const auto divider = layout.footer.x + text_width(items, FontStyle::secondary) + 18;
    canvas.fill({divider, baseline, 1, 18}, outline);
    std::string footer;
    if(!browser.message().empty())
        footer = browser.message();
    else if(state.stopping)
        footer = i18n::tr("browser.stopping");
    else if(browser.loading())
        footer = i18n::tr("browser.reading");
    else if(std::all_of(browser.entries().begin(), browser.entries().end(), [](auto& entry) { return entry.directory; }))
        footer = i18n::tr("browser.select_folder");
    else if(browser.selected() < browser.entries().size())
        footer = escape_path(browser.entries()[browser.selected()].path);
    const auto columns = static_cast<std::size_t>(std::max(1, (layout.card.width - 340) / 9));
    draw_text(canvas, divider + 18, baseline, fit_label(footer, columns), browser.message().empty() ? text : Color{255, 160, 130, 255}, FontStyle::secondary);
}
}  // namespace lumaframevr::ui
