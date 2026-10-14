#include "browser.hpp"
#include <algorithm>

namespace lumaframevr::ui {
namespace {
constexpr int sidebar_width = 228;
constexpr int row_height = 46;
constexpr int rows_first = 158;
}  // namespace
BrowserLayout browser_layout(int width, int height) {
    return {{6, 6, width - 12, height - 12},
            {sidebar_width + 10, rows_first, width - sidebar_width - 22, std::max(0, height - rows_first - 64)},
            {sidebar_width + 16, height - 56, width - sidebar_width - 32, 40}};
}
std::size_t BrowserLayout::capacity() const {
    const int span = footer.y - rows.y;
    return span < 0 ? 0 : static_cast<std::size_t>(span / row_height);
}
Rect BrowserLayout::row(std::size_t slot) const {
    return {rows.x, rows.y + static_cast<int>(slot) * row_height, rows.width, row_height - 4};
}
std::size_t list_scroll(std::size_t selected, std::size_t count, std::size_t rows) {
    if(!rows || rows >= count)
        return 0;
    return std::min(selected >= rows - 1 ? selected - (rows - 1) : 0, count - rows);
}
std::vector<std::filesystem::path> browser_ancestors(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> paths;
    for(auto path = directory; !path.empty();) {
        paths.push_back(path);
        const auto parent = path.parent_path();
        if(parent == path)
            break;
        path = parent;
    }
    std::reverse(paths.begin(), paths.end());
    return paths;
}
std::optional<std::size_t> browser_entry_at(int width, int height, std::size_t count, std::size_t selected, BrowserMode mode, int x, int y) {
    return browser_viewport(width, height, count, selected, mode).entry_at(x, y);
}
}  // namespace lumaframevr::ui
