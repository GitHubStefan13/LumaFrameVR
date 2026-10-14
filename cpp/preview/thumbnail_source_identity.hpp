#pragma once
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
namespace lumaframevr {
struct ThumbnailSourceIdentity {
    dev_t device{};
    ino_t inode{};
    off_t size{};
    std::int64_t modified_seconds{}, modified_nanos{}, changed_seconds{}, changed_nanos{};
    bool operator==(const ThumbnailSourceIdentity&) const = default;
};
inline ThumbnailSourceIdentity thumbnail_source_identity(const std::filesystem::path& path) {
    struct stat info{};
    if(stat(path.c_str(), &info) || !S_ISREG(info.st_mode))
        throw std::runtime_error("Thumbnail source identity unavailable");
    return {info.st_dev, info.st_ino, info.st_size, info.st_mtim.tv_sec, info.st_mtim.tv_nsec, info.st_ctim.tv_sec, info.st_ctim.tv_nsec};
}
inline std::string thumbnail_identity_key(const ThumbnailSourceIdentity& identity) {
    return std::to_string(identity.device) + ':' + std::to_string(identity.inode) + ':' + std::to_string(identity.size) + ':' + std::to_string(identity.modified_seconds) + ':' +
           std::to_string(identity.modified_nanos) + ':' + std::to_string(identity.changed_seconds) + ':' + std::to_string(identity.changed_nanos);
}
}  // namespace lumaframevr
