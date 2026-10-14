#include "browser.hpp"
#include <algorithm>
namespace lumaframevr::ui {
namespace {
constexpr int tile_width = 176, tile_height = 117, column_step = 186, row_step = 127;
}
BrowserViewport browser_viewport(int width, int height, std::size_t count, std::size_t selected, BrowserMode mode) {
    BrowserViewport viewport{browser_layout(width, height), mode};
    if(mode == BrowserMode::List) {
        viewport.columns = 1;
        viewport.rows = static_cast<int>(viewport.layout.capacity());
        viewport.first = list_scroll(selected, count, viewport.rows);
        viewport.count = std::min(count - viewport.first, static_cast<std::size_t>(viewport.rows));
        return viewport;
    }
    const auto area = viewport.layout.rows;
    viewport.columns = area.width < tile_width ? 0 : (area.width + 10) / column_step;
    viewport.rows = area.height < tile_height ? 0 : (area.height + 10) / row_step;
    if(!viewport.columns || !viewport.rows)
        return viewport;
    const auto columns = static_cast<std::size_t>(viewport.columns);
    const auto rows = static_cast<std::size_t>(viewport.rows);
    const auto capacity = columns * rows;
    const auto selected_row = selected / columns;
    const auto first_row = selected_row >= rows - 1 ? selected_row - (rows - 1) : 0;
    const auto total_rows = count / columns + (count % columns != 0);
    const auto last_row = total_rows > rows ? total_rows - rows : 0;
    viewport.first = capacity >= count ? 0 : std::min(first_row, last_row) * columns;
    viewport.count = std::min(capacity, count - viewport.first);
    return viewport;
}
Rect BrowserViewport::rectangle(std::size_t slot) const {
    if(slot >= count)
        throw std::out_of_range("Browser viewport slot");
    if(mode == BrowserMode::List)
        return layout.row(slot);
    return {layout.rows.x + static_cast<int>(slot % columns) * column_step, layout.rows.y + static_cast<int>(slot / columns) * row_step, tile_width, tile_height};
}
std::optional<std::size_t> BrowserViewport::entry_at(int x, int y) const {
    if(!count || x < layout.rows.x || y < layout.rows.y)
        return {};
    const int dx = x - layout.rows.x, dy = y - layout.rows.y;
    std::size_t slot;
    if(mode == BrowserMode::List) {
        // The pitch comes from the layout, not from a constant, or the mapping drifts the moment
        // the row geometry changes.
        const auto pitch = layout.row(1).y - layout.row(0).y;
        slot = pitch > 0 ? static_cast<std::size_t>(dy / pitch) : 0;
        if(slot >= count || !layout.row(slot).contains(x, y))
            return {};
    } else {
        const auto column = dx / column_step, row = dy / row_step;
        if(column >= columns || row >= rows || dx % column_step >= tile_width || dy % row_step >= tile_height)
            return {};
        slot = static_cast<std::size_t>(row * columns + column);
        if(slot >= count)
            return {};
    }
    return first + slot;
}
}  // namespace lumaframevr::ui
