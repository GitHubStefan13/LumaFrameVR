#pragma once
#include "thumbnail_services.hpp"
#include "thumbnail_persistence_owner.hpp"
#include <condition_variable>
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
namespace lumaframevr {
struct ThumbnailCache::State {
    using Key = std::pair<std::filesystem::path, unsigned>;
    struct Entry {
        std::shared_ptr<ThumbnailPixels> pixels;
        std::uint64_t used;
    };
    struct Job {
        Key key;
        std::uint64_t reset;
    };
    struct Completed {
        Job job;
        std::shared_ptr<ThumbnailPixels> pixels;
        std::string error;
    };
    std::map<Key, Entry> ready;
    std::map<Key, std::string> failures;
    TileWanted wanted;
    std::uint64_t tick{}, reset_serial{}, revision{};
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<Job> pending;
    std::deque<Completed> completed;
    bool stopped{}, reset_worker{};
    std::atomic_size_t source_count{};
    std::optional<std::filesystem::path> disk_directory;
    std::size_t disk_budget{128 * 1024 * 1024};
    std::uint64_t disk_serial{};
    std::shared_ptr<ThumbnailPersistenceOwner> disk_owner{std::make_shared<ThumbnailPersistenceOwner>()};
    std::atomic_uint64_t disk_hits{};
    std::string disk_error;
    std::thread worker;
    void run();
    void evict();
    std::size_t bytes() const;
};
}  // namespace lumaframevr
