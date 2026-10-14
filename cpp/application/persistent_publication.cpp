#include "persistent_directory.hpp"
#include "persistent_descriptor.hpp"
#include <atomic>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>
namespace lumaframevr {
namespace {
std::string unique_name(std::string_view prefix, std::string_view leaf) {
    static std::atomic_uint64_t serial{};
    return std::string(prefix) + std::string(leaf) + "-" + std::to_string(getpid()) + "-" + std::to_string(++serial);
}
void move_record(int parent, const std::string& from, const std::string& to) {
    if(syscall(SYS_renameat2, parent, from.c_str(), parent, to.c_str(), RENAME_NOREPLACE))
        persistent_error();
}
}  // namespace
void PersistentDirectory::write(std::string_view name, std::string_view bytes, const std::optional<std::string>& expected, const PersistentPublish& publish) const {
    const auto leaf = persistent_name(name);
    if(expected)
        write(unique_name(".prior-", leaf), *expected);
    const auto staging = unique_name(".prepared-", leaf);
    PersistentDescriptor file(openat(descriptor_, staging.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    std::size_t offset{};
    while(offset < bytes.size()) {
        const auto count = ::write(file.get(), bytes.data() + offset, bytes.size() - offset);
        if(count < 0 && errno == EINTR)
            continue;
        if(count <= 0)
            throw std::runtime_error("Cannot write persistent record");
        offset += static_cast<std::size_t>(count);
    }
    if(fsync(file.get()))
        throw std::runtime_error("Cannot sync persistent record");
    if(expected) {
        const auto displaced = unique_name(".displaced-", leaf);
        move_record(descriptor_, leaf, displaced);
        if(fsync(descriptor_))
            throw std::runtime_error("Cannot sync retained displaced record");
        PersistentDescriptor retained(openat(descriptor_, displaced.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
        struct stat info{};
        if(fstat(retained.get(), &info))
            persistent_error();
        persistent_regular(info);
        if(fsync(retained.get()))
            throw std::runtime_error("Cannot sync retained displaced contents");
        if(read(displaced, expected->size()) != expected)
            throw std::runtime_error("Persistent record changed; displaced bytes preserved");
    }
    const auto commit = [&] { move_record(descriptor_, staging, leaf); };
    if(publish) {
        if(!publish(commit))
            throw std::runtime_error("Persistent publication cancelled; prepared bytes retained");
    } else
        commit();
    if(fsync(descriptor_))
        throw std::runtime_error("Cannot sync published record directory");
}
}  // namespace lumaframevr
