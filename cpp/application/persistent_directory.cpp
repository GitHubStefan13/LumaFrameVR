#include "persistent_directory.hpp"
#include "persistent_descriptor.hpp"
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <sys/file.h>
namespace lumaframevr {
PersistentDirectory::PersistentDirectory(const std::filesystem::path& path, bool create) {
    if(!path.is_absolute())
        throw std::runtime_error("Persistent storage requires an absolute path");
    for(const auto& part : path)
        if(part == "..")
            throw std::runtime_error("Persistent storage traversal refused");
    PersistentDescriptor directory(open("/", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    for(const auto& part : path.relative_path()) {
        if(part == "." || part.empty())
            continue;
        int next = openat(directory.get(), part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if(next < 0 && errno == ENOENT) {
            if(!create)
                return;
            if(mkdirat(directory.get(), part.c_str(), 0700) && errno != EEXIST)
                persistent_error();
            if(fsync(directory.get()))
                persistent_error();
            next = openat(directory.get(), part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        }
        directory.reset(next);
    }
    struct stat info{};
    if(fstat(directory.get(), &info))
        persistent_error();
    if(info.st_uid != getuid())
        throw std::runtime_error("Persistent storage is owned by another user");
    descriptor_ = directory.release();
}
PersistentDirectory::~PersistentDirectory() {
    if(descriptor_ >= 0)
        close(descriptor_);
}
void PersistentDirectory::check(std::string_view name) const {
    const auto leaf = persistent_name(name);
    if(descriptor_ < 0)
        return;
    struct stat info{};
    if(fstatat(descriptor_, leaf.c_str(), &info, AT_SYMLINK_NOFOLLOW)) {
        if(errno == ENOENT)
            return;
        persistent_error();
    }
    if(S_ISDIR(info.st_mode)) {
        if(info.st_uid != getuid())
            throw std::runtime_error("Persistent directory ownership refused");
    } else
        persistent_regular(info);
}
std::optional<std::string> PersistentDirectory::read(std::string_view name, std::size_t limit) const {
    const auto leaf = persistent_name(name);
    if(descriptor_ < 0)
        return {};
    const int opened = openat(descriptor_, leaf.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if(opened < 0 && errno == ENOENT)
        return {};
    PersistentDescriptor file(opened);
    struct stat info{};
    if(fstat(file.get(), &info))
        persistent_error();
    persistent_regular(info);
    if(info.st_size < 0 || static_cast<std::uint64_t>(info.st_size) > limit)
        throw std::runtime_error("Invalid persistent file size");
    std::string bytes(static_cast<std::size_t>(info.st_size), '\0');
    std::size_t offset{};
    while(offset < bytes.size()) {
        const auto count = ::read(file.get(), bytes.data() + offset, bytes.size() - offset);
        if(count < 0 && errno == EINTR)
            continue;
        if(count <= 0)
            throw std::runtime_error("Incomplete persistent record");
        offset += static_cast<std::size_t>(count);
    }
    char extra;
    if(::read(file.get(), &extra, 1) != 0)
        throw std::runtime_error("Persistent record changed during read");
    return bytes;
}
std::size_t PersistentDirectory::retained_bytes(std::size_t limit) const {
    PersistentDescriptor copy(openat(descriptor_, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    struct DirectoryCloser {
        void operator()(DIR* value) const { closedir(value); }
    };
    std::unique_ptr<DIR, DirectoryCloser> entries(fdopendir(copy.get()));
    if(!entries)
        persistent_error();
    copy.release();
    std::size_t count{};
    errno = 0;
    while(const auto* entry = readdir(entries.get())) {
        const std::string_view name(entry->d_name);
        if(name == "." || name == "..")
            continue;
        struct stat info{};
        if(fstatat(descriptor_, entry->d_name, &info, AT_SYMLINK_NOFOLLOW))
            persistent_error();
        persistent_regular(info);
        if(info.st_size < 0)
            throw std::runtime_error("Invalid retained cache size");
        const auto size = static_cast<std::uint64_t>(info.st_size);
        if(size > limit || count > limit - size)
            return limit;
        count += static_cast<std::size_t>(size);
        errno = 0;
    }
    if(errno)
        persistent_error();
    return count;
}
void PersistentDirectory::lock_writer() const {
    if(flock(descriptor_, LOCK_EX | LOCK_NB))
        throw std::runtime_error("Thumbnail cache already has a persistence writer");
}
}  // namespace lumaframevr
