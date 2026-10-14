#include "browser.hpp"
#include "browser_colors.hpp"
#include "catalogue.hpp"
#include "font_style.hpp"
#include "icons.hpp"
#include <algorithm>
#include <string>

namespace lumaframevr::ui {
namespace {
using namespace browser_color;
constexpr int sidebar_width = 228;
// Which mark a control carries. The set is the baked Lucide one (scripts/make-icon-atlas.py).
std::string_view action_icon(BrowserAction action) {
    switch(action) {
        case BrowserAction::Back:
            return "arrow_back";
        case BrowserAction::Forward:
            return "arrow_forward";
        case BrowserAction::Up:
            return "arrow_up";
        case BrowserAction::Refresh:
            return "refresh";
        case BrowserAction::Settings:
            return "settings";
        case BrowserAction::Passthrough:
            return "eye";
        case BrowserAction::Quit:
            return "close";
        case BrowserAction::Sort:
            return "chevron_down";
        case BrowserAction::List:
            return "list";
        case BrowserAction::Grid:
            return "grid";
        default:
            return {};
    }
}
// An icon, a label, or both, centred on a rounded plate when the control is active.
void draw_control(Canvas& canvas, const BrowserTarget& target, bool show_label) {
    const auto ink = target.enabled ? text : dim;
    if(target.active)
        canvas.rounded(target.area, target.area.height / 2, selected);
    const auto mark = action_icon(target.action);
    const auto label = show_label ? std::string_view(target.label) : std::string_view{};
    const auto span = (mark.empty() ? 0 : 22) + (mark.empty() || label.empty() ? 0 : 8) + (label.empty() ? 0 : text_width(label, FontStyle::medium));
    auto x = target.area.x + std::max(0, (target.area.width - span) / 2);
    if(!mark.empty()) {
        draw_icon(canvas, {x, target.area.y + (target.area.height - 22) / 2, 22, 22}, mark, ink);
        x += 30;
    }
    if(!label.empty())
        draw_text(canvas, x, target.area.y + (target.area.height - text_height(FontStyle::medium)) / 2, label, ink, FontStyle::medium);
}
std::string_view place_icon(std::string_view label) {
    // Home and the file system are the places every desktop has; anything else is a folder.
    if(label.find("Home") != std::string_view::npos)
        return "home";
    if(label.find("System") != std::string_view::npos)
        return "drive";
    return "folder";
}
}  // namespace
void draw_browser_chrome(Canvas& canvas, const FolderBrowser& browser, const BrowserRenderState& state) {
    const auto targets = browser_targets(browser, canvas.width(), canvas.height(), state);
    canvas.rounded({6, 6, canvas.width() - 12, canvas.height() - 12}, 18, content);
    canvas.fill({6, 6, sidebar_width, canvas.height() - 12}, places);
    canvas.fill({sidebar_width, 6, canvas.width() - sidebar_width - 6, 64}, toolbar);
    canvas.fill({sidebar_width, 126, canvas.width() - sidebar_width - 6, 28}, band);
    draw_text(canvas, 20, 72, i18n::tr("browser.places"), dim, FontStyle::secondary);
    draw_text(canvas, sidebar_width + 228, 6 + (64 - text_height(FontStyle::title)) / 2, i18n::tr("browser.folders"), text, FontStyle::title);
    draw_text(canvas, sidebar_width + 60, 132, i18n::tr("browser.name"), dim, FontStyle::secondary);
    for(const auto& target : targets) {
        switch(target.action) {
            case BrowserAction::Place:
                if(target.active)
                    canvas.rounded(target.area, target.area.height / 2, highlight);
                draw_icon(canvas, {target.area.x + 10, target.area.y + 10, 24, 24}, place_icon(target.label), folder);
                draw_text(canvas, target.area.x + 46, target.area.y + (target.area.height - text_height(FontStyle::medium)) / 2, target.label, text, FontStyle::medium);
                break;
            case BrowserAction::Breadcrumb:
                // The open folder is the one that is filled; the ones above it stay quiet, and only
                // they get a separator after them.
                if(target.active)
                    canvas.rounded(target.area, target.area.height / 2, accent);
                draw_text(canvas, target.area.x + (target.area.width - text_width(target.label, FontStyle::medium)) / 2, target.area.y + (target.area.height - text_height(FontStyle::medium)) / 2,
                          target.label, target.active ? Color{255, 255, 255, 255} : dim, FontStyle::medium);
                if(!target.active)
                    draw_icon(canvas, {target.area.right() + 4, target.area.y + 14, 18, 18}, "chevron_right", dim);
                break;
            case BrowserAction::List:
            case BrowserAction::Grid:
                canvas.rounded(target.area, target.area.height / 2, target.active ? accent : highlight);
                draw_control(canvas, target, true);
                break;
            default:
                draw_control(canvas, target, target.shows_label);
                break;
        }
    }
}
}  // namespace lumaframevr::ui
