#include "application_controller.hpp"
namespace lumaframevr::application {
void Controller::adjacent_image(int direction) {
    const auto& entries = explorer.browser.entries();
    const auto current = std::filesystem::absolute(path).lexically_normal();
    std::ptrdiff_t position = direction > 0 ? -1 : static_cast<std::ptrdiff_t>(entries.size());
    for(std::size_t i = 0; i < entries.size(); ++i) {
        if(entries[i].path.lexically_normal() == current) {
            position = static_cast<std::ptrdiff_t>(i);
            break;
        }
    }
    for(position += direction; position >= 0 && position < static_cast<std::ptrdiff_t>(entries.size()); position += direction) {
        const auto& item = entries[static_cast<std::size_t>(position)];
        if(!item.directory && is_image_path(item.path)) {
            select_entry(static_cast<std::size_t>(position));
            return;
        }
    }
}
}  // namespace lumaframevr::application
