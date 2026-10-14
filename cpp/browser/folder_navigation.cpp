#include "folder_browser.hpp"
#include <algorithm>

namespace lumaframevr {
std::filesystem::path FolderBrowser::selected_path() const {
    return selected_ < entries_.size() ? entries_[selected_].path : std::filesystem::path{};
}
void FolderBrowser::remember() {
    if(selected_ >= entries_.size())
        return;
    if(selections_.size() >= 128 && !selections_.contains(directory_))
        selections_.clear();
    selections_[directory_] = entries_[selected_].path;
}
bool FolderBrowser::select(std::size_t index) {
    if(index >= entries_.size())
        return false;
    selected_ = index;
    remember();
    return true;
}
void FolderBrowser::move_selection(std::ptrdiff_t direction) {
    const auto index = static_cast<std::ptrdiff_t>(selected_);
    const auto maximum = entries_.empty() ? 0 : static_cast<std::ptrdiff_t>(entries_.size() - 1);
    select(static_cast<std::size_t>(std::clamp(index + direction, std::ptrdiff_t{0}, maximum)));
}
void FolderBrowser::push_history(std::vector<std::filesystem::path>& history, const std::filesystem::path& path) {
    if(history.size() >= 64)
        history.erase(history.begin());
    history.push_back(path);
}
void FolderBrowser::go(const std::filesystem::path& path) {
    std::error_code error;
    if(!std::filesystem::is_directory(path, error)) {
        message_ = "Cannot open folder: " + path.native();
        return;
    }
    if(path == directory_) {
        refresh();
        return;
    }
    push_history(back_, directory_);
    forward_.clear();
    load(path);
}
void FolderBrowser::parent() {
    if(directory_.has_parent_path())
        go(directory_.parent_path());
}
void FolderBrowser::back() {
    if(back_.empty())
        return;
    auto path = back_.back();
    back_.pop_back();
    push_history(forward_, directory_);
    load(path);
}
void FolderBrowser::forward() {
    if(forward_.empty())
        return;
    auto path = forward_.back();
    forward_.pop_back();
    push_history(back_, directory_);
    load(path);
}
void FolderBrowser::refresh() {
    places_ = browser_places();
    load(directory_);
}
void FolderBrowser::toggle_hidden() {
    hidden_ = !hidden_;
    refresh();
}
void FolderBrowser::toggle_sort() {
    descending_ = !descending_;
    sort();
}
std::optional<std::filesystem::path> FolderBrowser::open() {
    if(loading_ || selected_ >= entries_.size())
        return std::nullopt;
    const auto entry = entries_[selected_];
    if(entry.directory) {
        go(entry.path);
        return std::nullopt;
    }
    std::error_code error;
    const auto status = std::filesystem::status(entry.path, error);
    if(!error && !std::filesystem::is_regular_file(status)) {
        message_ = "Not a regular file: video opening is disabled for devices, pipes and sockets";
        return std::nullopt;
    }
    return entry.path;
}
}  // namespace lumaframevr
