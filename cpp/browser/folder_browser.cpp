#include "folder_scan_state.hpp"
#include <algorithm>

namespace lumaframevr {
FolderBrowser::FolderBrowser(std::filesystem::path directory) : places_(browser_places()), scan_(std::make_shared<ScanState>()) {
    thread_ = std::thread([state = scan_] { scan_loop(state); });
    try {
        load(directory);
    } catch(...) {
        {
            std::lock_guard lock(scan_->mutex);
            scan_->stopped = true;
        }
        scan_->changed.notify_one();
        thread_.join();
        throw;
    }
}
FolderBrowser::~FolderBrowser() {
    {
        std::lock_guard lock(scan_->mutex);
        scan_->stopped = true;
    }
    scan_->changed.notify_one();
    if(thread_.joinable())
        thread_.join();
}
void FolderBrowser::scan_loop(const std::shared_ptr<ScanState>& state) {
    for(;;) {
        std::unique_lock lock(state->mutex);
        state->changed.wait(lock, [&] { return state->stopped || state->requested.has_value(); });
        if(state->stopped)
            return;
        auto request = std::move(*state->requested);
        state->requested.reset();
        lock.unlock();
        ScanState::Reply reply{{}, {}, request.generation};
        try {
            reply.entries = scan_folder(request.directory, request.hidden, [&] { return state->stopped || state->generation != request.generation; });
        } catch(const std::exception& error) {
            reply.error = error.what();
        }
        lock.lock();
        if(!state->stopped && state->generation == request.generation)
            state->reply = std::move(reply);
    }
}
void FolderBrowser::load(const std::filesystem::path& path) {
    directory_ = path;
    entries_.clear();
    if(path.has_parent_path() && path.parent_path() != path)
        entries_.push_back({path.parent_path(), true});
    selected_ = 0;
    message_.clear();
    loading_ = true;
    std::lock_guard lock(scan_->mutex);
    const auto generation = ++scan_->generation;
    scan_->reply.reset();
    scan_->requested = ScanState::Request{directory_, hidden_, generation};
    scan_->changed.notify_one();
}
bool FolderBrowser::poll() {
    std::optional<ScanState::Reply> reply;
    {
        std::lock_guard lock(scan_->mutex);
        reply = std::move(scan_->reply);
        scan_->reply.reset();
    }
    if(!reply || reply->generation != scan_->generation)
        return false;
    loading_ = false;
    message_ = std::move(reply->error);
    if(!message_.empty())
        return true;
    const auto first = std::min(entries_.size(), reply->entries.size());
    entries_.insert(entries_.end(), std::make_move_iterator(reply->entries.begin()), std::make_move_iterator(reply->entries.end()));
    selected_ = first;
    auto remembered = selections_.find(directory_);
    if(remembered != selections_.end()) {
        auto found = std::find_if(entries_.begin(), entries_.end(), [&](auto& entry) { return entry.path == remembered->second; });
        if(found != entries_.end())
            selected_ = static_cast<std::size_t>(found - entries_.begin());
    }
    sort();
    remember();
    return true;
}
void FolderBrowser::sort() {
    const auto path = selected_path();
    const auto parent = directory_.parent_path();
    std::sort(entries_.begin(), entries_.end(), [&](auto& a, auto& b) {
        if((a.path == parent) != (b.path == parent))
            return a.path == parent;
        if(a.directory != b.directory)
            return a.directory;
        return descending_ ? b.path < a.path : a.path < b.path;
    });
    auto found = std::find_if(entries_.begin(), entries_.end(), [&](auto& entry) { return entry.path == path; });
    selected_ = found == entries_.end() ? 0 : static_cast<std::size_t>(found - entries_.begin());
}
}  // namespace lumaframevr
