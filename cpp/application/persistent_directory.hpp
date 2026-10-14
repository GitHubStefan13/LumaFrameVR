#pragma once
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
namespace lumaframevr {
using PersistentPublish = std::function<bool(const std::function<void()>&)>;
class PersistentDirectory {
   public:
    explicit PersistentDirectory(const std::filesystem::path& path, bool create = false);
    ~PersistentDirectory();
    PersistentDirectory(const PersistentDirectory&) = delete;
    PersistentDirectory& operator=(const PersistentDirectory&) = delete;
    int descriptor() const { return descriptor_; }
    void check(std::string_view name) const;
    std::optional<std::string> read(std::string_view name, std::size_t limit) const;
    void write(std::string_view name, std::string_view bytes, const std::optional<std::string>& expected = {}, const PersistentPublish& publish = {}) const;
    std::size_t retained_bytes(std::size_t limit) const;
    void lock_writer() const;

   private:
    int descriptor_{-1};
};
}  // namespace lumaframevr
