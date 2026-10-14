#pragma once
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
namespace lumaframevr {
inline void persistent_error() {
    throw std::runtime_error(std::strerror(errno));
}
class PersistentDescriptor {
   public:
    explicit PersistentDescriptor(int value) : value_(value) {
        if(value < 0)
            persistent_error();
    }
    ~PersistentDescriptor() {
        if(value_ >= 0)
            close(value_);
    }
    int get() const { return value_; }
    int release() { return std::exchange(value_, -1); }
    void reset(int value) {
        PersistentDescriptor next(value);
        close(value_);
        value_ = next.release();
    }

   private:
    int value_;
};
inline void persistent_regular(const struct stat& info) {
    if(!S_ISREG(info.st_mode) || info.st_nlink != 1 || info.st_uid != getuid())
        throw std::runtime_error("Invalid persistent file type or ownership");
}
inline std::string persistent_name(std::string_view name) {
    if(name.empty() || name == "." || name == ".." || name.find('/') != name.npos || name.find('\0') != name.npos)
        throw std::runtime_error("Invalid persistent leaf name");
    return std::string(name);
}
}  // namespace lumaframevr
