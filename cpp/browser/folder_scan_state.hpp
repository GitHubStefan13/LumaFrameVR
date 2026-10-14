#pragma once
#include "folder_browser.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace lumaframevr {
struct FolderBrowser::ScanState {
    std::mutex mutex;
    std::condition_variable changed;
    std::atomic<std::uint64_t> generation{};
    std::atomic<bool> stopped{};
    struct Request {
        std::filesystem::path directory;
        bool hidden;
        std::uint64_t generation;
    };
    struct Reply {
        std::vector<FolderEntry> entries;
        std::string error;
        std::uint64_t generation;
    };
    std::optional<Request> requested;
    std::optional<Reply> reply;
};
}  // namespace lumaframevr
