#pragma once
#include <filesystem>
#include <optional>
#include <string>
namespace lumaframevr {
struct ViewerPreferences {
    bool show_hidden{true}, save_thumbnails{}, remember_last_state{true};
    std::filesystem::path folder;
    float volume{0.8F};
    bool muted{}, grid{}, descending{};
    std::string language{"en"};
    bool operator==(const ViewerPreferences&) const = default;
};
std::optional<std::filesystem::path> viewer_state_path();
std::filesystem::path viewer_thumbnail_path(const std::filesystem::path& state);
class ViewerPreferenceStore {
   public:
    explicit ViewerPreferenceStore(std::optional<std::filesystem::path> path = {});
    ViewerPreferences load();
    bool save(const ViewerPreferences& preferences);
    bool existing() const;
    const std::string& error() const { return error_; }

   private:
    std::optional<std::filesystem::path> path_;
    std::optional<ViewerPreferences> saved_;
    std::optional<std::string> saved_bytes_;
    std::string error_;
    bool writable_{true};
};
}  // namespace lumaframevr
