#include "viewer_preferences.hpp"
#include "persistent_files.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
namespace lumaframevr {
namespace {
constexpr auto identity = "LumaFrameVR viewer state 1";
bool boolean(int value) {
    return value == 0 || value == 1;
}
ViewerPreferences remembered(ViewerPreferences value) {
    if(!value.remember_last_state) {
        value.folder.clear();
        value.volume = .8F;
        value.muted = false;
    }
    return value;
}
}  // namespace
ViewerPreferenceStore::ViewerPreferenceStore(std::optional<std::filesystem::path> path) : path_(std::move(path)) {}
ViewerPreferences ViewerPreferenceStore::load() {
    ViewerPreferences result;
    if(!path_)
        return result;
    try {
        const auto bytes = read_persistent_file(*path_, 16384);
        if(!bytes)
            return result;
        std::istringstream input(*bytes);
        std::string header, folder, language;
        int hidden{}, cache{}, remember{}, muted{}, grid{}, descending{}, sizes{};
        std::getline(input, header);
        if(header != identity || !(input >> hidden >> cache >> remember >> result.volume >> muted >> grid >> descending >> std::quoted(folder)) || !boolean(hidden) || !boolean(cache) ||
           !boolean(remember) || !boolean(muted) || !boolean(grid) || !boolean(descending) || !std::isfinite(result.volume) || result.volume < 0 || result.volume > 1 || folder.size() > 8192)
            throw std::runtime_error("Unrecognized or invalid viewer state; existing file preserved");
        // The tail carries a boolean and then the language, and both are optional: a state file
        // written before the file-size switch existed ends at the language, so a token that is not a
        // boolean is that language rather than a malformed size.
        if(!(input >> std::ws).eof()) {
            std::string token;
            input >> token;
            if(token == "0" || token == "1") {
                sizes = token == "1";
                if(!(input >> std::ws).eof())
                    input >> language;
            } else {
                language = token;
            }
        }
        input >> std::ws;
        if(!input.eof())
            throw std::runtime_error("Unexpected viewer state content; existing file preserved");
        if(language.size() > 16 || (!language.empty() && !std::all_of(language.begin(), language.end(), [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; })))
            throw std::runtime_error("Invalid language code in viewer state; existing file preserved");
        result.show_hidden = hidden;
        result.save_thumbnails = cache;
        result.remember_last_state = remember;
        result.show_file_sizes = boolean(sizes) && sizes == 1;
        result.muted = muted;
        result.grid = grid;
        result.descending = descending;
        result.folder = folder;
        if(!language.empty())
            result.language = language;
        result = remembered(result);
        saved_ = result;
        saved_bytes_ = bytes;
    } catch(const std::exception& error) {
        error_ = error.what();
        writable_ = false;
        result = {};
    }
    return result;
}
bool ViewerPreferenceStore::save(const ViewerPreferences& preferences) {
    if(!path_)
        return true;
    if(!writable_)
        return false;
    if(!std::isfinite(preferences.volume) || preferences.volume < 0 || preferences.volume > 1) {
        error_ = "Invalid saved volume";
        return false;
    }
    const auto value = remembered(preferences);
    if(saved_ == value)
        return true;
    try {
        ViewerPreferenceStore existing(path_);
        existing.load();
        if(!existing.error().empty())
            throw std::runtime_error(existing.error());
        if(saved_bytes_ && existing.saved_bytes_ != saved_bytes_)
            throw std::runtime_error("Viewer state changed externally; existing bytes preserved");
        std::ostringstream output;
        output << identity << '\n'
               << value.show_hidden << ' ' << value.save_thumbnails << ' ' << value.remember_last_state << ' ' << std::setprecision(std::numeric_limits<float>::max_digits10) << value.volume << ' '
               << value.muted << ' ' << value.grid << ' ' << value.descending << ' ' << std::quoted(value.folder.string()) << ' ' << value.show_file_sizes << ' ' << value.language << '\n';
        const auto bytes = output.str();
        write_persistent_file(*path_, bytes, existing.saved_bytes_);
        saved_ = value;
        saved_bytes_ = bytes;
        error_.clear();
        return true;
    } catch(const std::exception& error) {
        error_ = error.what();
        writable_ = false;
        return false;
    }
}
bool ViewerPreferenceStore::existing() const {
    return path_ && std::filesystem::is_regular_file(*path_);
}
std::optional<std::filesystem::path> viewer_state_path() {
    if(const auto* override = std::getenv("LUMAFRAMEVR_VIEWER_STATE"))
        return *override ? std::optional<std::filesystem::path>(override) : std::nullopt;
    if(const auto* config = std::getenv("XDG_CONFIG_HOME"); config && std::filesystem::path(config).is_absolute())
        return std::filesystem::path(config) / "lumaframevr/viewer.state";
    if(const auto* home = std::getenv("HOME"); home && std::filesystem::path(home).is_absolute())
        return std::filesystem::path(home) / ".config/lumaframevr/viewer.state";
    return {};
}
std::filesystem::path viewer_thumbnail_path(const std::filesystem::path& state) {
    if(const auto* cache = std::getenv("XDG_CACHE_HOME"); cache && std::filesystem::path(cache).is_absolute())
        return std::filesystem::path(cache) / "lumaframevr/thumbnails-v1";
    return state.parent_path() / "thumbnails-v1";
}
}  // namespace lumaframevr
