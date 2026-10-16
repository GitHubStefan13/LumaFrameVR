#include "folder_browser.hpp"
#include <algorithm>

namespace lumaframevr {
std::vector<FolderEntry> scan_folder(const std::filesystem::path& directory, bool hidden, const std::function<bool()>& cancelled) {
    std::vector<FolderEntry> entries;
    for(const auto& file : std::filesystem::directory_iterator(directory)) {
        if(cancelled())
            break;
        const auto name = file.path().filename().native();
        if(!hidden && !name.empty() && name.front() == '.')
            continue;
        std::error_code error;
        const bool directory = std::filesystem::is_directory(file.path(), error);
        // One stat during the scan, reused by every frame that lists it: a size that is wrong when it
        // cannot be read is zero, which draws as nothing rather than as a made-up number.
        const auto size = directory ? std::uintmax_t{0} : std::filesystem::file_size(file.path(), error);
        entries.push_back({file.path(), directory, error ? std::uintmax_t{0} : size});
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        if(a.directory != b.directory)
            return a.directory;
        return a.path < b.path;
    });
    return entries;
}
}  // namespace lumaframevr
