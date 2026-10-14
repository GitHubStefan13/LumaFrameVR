#include "browser.hpp"
#include "font_style.hpp"
#include "catalogue.hpp"
#include <algorithm>

namespace lumaframevr::ui {
namespace {
// The header controls and the two list/grid pills under the breadcrumbs. The drawing and the beam
// hit testing both come through browser_targets, so a control can never be drawn where it cannot
// be clicked, and can never be clicked where it is not drawn.
constexpr int sidebar_width = 228;
constexpr int header_height = 64;
constexpr int crumb_row = 78;
constexpr int crumb_height = 44;
}  // namespace
std::vector<BrowserTarget> browser_targets(const FolderBrowser& browser, int width, int height, const BrowserRenderState& state) {
    using Action = BrowserAction;
    const auto& directory = browser.directory();
    const bool parent = directory.has_parent_path() && directory.parent_path() != directory;
    const int centre = 6 + header_height / 2;
    // The controls on the right are laid out from the edge inwards. Sizing them from their
    // labels is only half the job: a measured control still has to be placed so that a longer
    // translation cannot overlap its neighbour.
    std::vector<BrowserTarget> targets{{{sidebar_width + 16, centre - 20, 40, 40}, Action::Back, 0, "<", browser.can_back(), false, false},
                                       {{sidebar_width + 64, centre - 20, 40, 40}, Action::Forward, 0, ">", browser.can_forward(), false, false},
                                       {{sidebar_width + 112, centre - 20, 40, 40}, Action::Up, 0, "^", parent, false, false},
                                       {{sidebar_width + 160, centre - 20, 40, 40}, Action::Refresh, 0, "R", true, false, false}};
    const auto label_width = [](std::string_view label) { return control_width(label, FontStyle::medium, true); };
    std::vector<BrowserTarget> right{
            {{0, centre - 20, label_width(i18n::tr("browser.quit")), 40}, Action::Quit, 0, i18n::tr("browser.quit"), true},
            {{0, centre - 20, label_width(i18n::tr("browser.settings")), 40}, Action::Settings, 0, i18n::tr("browser.settings"), true, state.viewer_settings},
            {{0, centre - 20, control_width(state.descending ? "Z-A" : "A-Z", FontStyle::medium) + 30, 40}, Action::Sort, 0, state.descending ? "Z-A" : "A-Z", true, state.descending},
            {{0, centre - 20, 40, 40}, Action::Passthrough, 0, i18n::tr("browser.passthrough"), state.passthrough_supported, state.passthrough_supported && state.passthrough_enabled, false}};
    int cursor = width - 8;
    for(auto& target : right) {
        cursor -= target.area.width;
        target.area.x = cursor;
        cursor -= 8;
        targets.push_back(target);
    }
    std::vector<BrowserTarget> views{{{0, crumb_row, label_width(i18n::tr("browser.list")), crumb_height}, Action::List, 0, i18n::tr("browser.list"), true, state.mode == BrowserMode::List},
                                     {{0, crumb_row, label_width(i18n::tr("browser.grid")), crumb_height}, Action::Grid, 0, i18n::tr("browser.grid"), true, state.mode == BrowserMode::Grid}};
    cursor = width - 8;
    for(auto& target : views) {
        cursor -= target.area.width;
        target.area.x = cursor;
        cursor -= 10;
        targets.push_back(target);
    }
    const auto views_left = cursor;
    int y = 96;
    for(std::size_t index = 0; index < browser.places().size(); ++index) {
        if(y + 44 >= height - 60)
            break;
        const auto& [label, path] = browser.places()[index];
        targets.push_back({{14, y, sidebar_width - 28, 44}, Action::Place, index, label, true, path == directory});
        y += 50;
    }
    // The breadcrumb is laid out from the right so the deepest folder is always visible, and the
    // folder that is open is the one that gets the filled pill.
    const auto paths = browser_ancestors(directory);
    std::vector<BrowserTarget> crumbs;
    int used = 0;
    for(std::size_t remaining = paths.size(); remaining; --remaining) {
        const auto index = remaining - 1;
        const auto filename = paths[index].filename().string();
        const auto label = fit_label(filename.empty() ? "/" : filename, 18);
        const auto crumb_width = text_width(label, FontStyle::medium) + 32;
        if(used + crumb_width + 28 > views_left - sidebar_width && !crumbs.empty())
            break;
        used += crumb_width + 28;
        crumbs.push_back({{0, crumb_row, crumb_width, crumb_height}, Action::Breadcrumb, index, label, true, index + 1 == paths.size()});
    }
    std::reverse(crumbs.begin(), crumbs.end());
    int x = sidebar_width + 16;
    for(auto& crumb : crumbs) {
        crumb.area.x = x;
        x += crumb.area.width + 28;
        targets.push_back(std::move(crumb));
    }
    return targets;
}
std::optional<BrowserTarget> browser_target_at(const FolderBrowser& browser, int width, int height, const BrowserRenderState& state, int x, int y) {
    for(const auto& target : browser_targets(browser, width, height, state))
        if(target.enabled && target.area.contains(x, y))
            return target;
    return {};
}
}  // namespace lumaframevr::ui
