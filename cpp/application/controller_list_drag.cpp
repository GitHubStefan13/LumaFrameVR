#include "application_controller.hpp"
#include <algorithm>
namespace lumaframevr::application {
void Controller::drag_list(const Hit& hit) {
    auto& drag = *list_drag_;
    drag.carry += hit.y - drag.last_y;
    drag.last_y = hit.y;
    const auto count = explorer.browser.entries().size();
    const auto viewport = ui::browser_viewport(std::min(hit.width, ui::browser_listing_width), hit.height, count, explorer.browser.selected(), explorer.view);
    const int pitch = explorer.view == ui::BrowserMode::List ? 36 : 127;
    const auto columns = static_cast<std::size_t>(std::max(1, viewport.columns));
    const auto rows = static_cast<std::size_t>(std::max(0, viewport.rows));
    const int steps = drag.carry / pitch;
    if(!steps)
        return;
    drag.carry -= steps * pitch;
    drag.scrolled = true;
    if(count <= rows * columns || rows == 0)
        return;
    const auto first = ui::list_scroll(explorer.browser.selected() / columns, (count + columns - 1) / columns, rows);
    const auto last = (count + columns - 1) / columns - rows;
    const auto next = static_cast<std::size_t>(std::clamp(static_cast<std::ptrdiff_t>(first) - steps, std::ptrdiff_t{0}, static_cast<std::ptrdiff_t>(last)));
    if(next != first)
        explorer.browser.select(std::min((next + rows - 1) * columns, count - 1));
    changed = true;
    ++revision;
}
}  // namespace lumaframevr::application
