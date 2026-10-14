#include "persistent_files.hpp"
#include "persistent_directory.hpp"
namespace lumaframevr {
void check_persistent_path(const std::filesystem::path& path) {
    PersistentDirectory parent(path.parent_path());
    parent.check(path.filename().string());
}
std::optional<std::string> read_persistent_file(const std::filesystem::path& path, std::size_t limit) {
    PersistentDirectory parent(path.parent_path());
    return parent.read(path.filename().string(), limit);
}
void write_persistent_file(const std::filesystem::path& path, std::string_view bytes, const std::optional<std::string>& expected) {
    PersistentDirectory parent(path.parent_path(), true);
    parent.write(path.filename().string(), bytes, expected);
}
}  // namespace lumaframevr
