#include "folder_browser.hpp"
#include "catalogue.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>

namespace lumaframevr {
std::vector<Place> browser_places() {
    const char* home_variable = std::getenv("HOME");
    const auto home = std::filesystem::path(home_variable ? home_variable : "/");
    const char* config_variable = std::getenv("XDG_CONFIG_HOME");
    const auto config = config_variable ? std::filesystem::path(config_variable) : home / ".config";
    std::map<std::string, std::string> directories;
    std::ifstream input(config / "user-dirs.dirs");
    for(std::string line; std::getline(input, line);) {
        auto equals = line.find('=');
        if(equals != std::string::npos)
            directories.emplace(line.substr(0, equals), line.substr(equals + 1));
    }
    std::vector<Place> places{{i18n::tr("place.home"), home}};
    for(const auto& row : {std::array<std::string, 3>{"place.videos", "Videos", "VIDEOS"}, std::array<std::string, 3>{"place.downloads", "Downloads", "DOWNLOAD"},
                           std::array<std::string, 3>{"place.documents", "Documents", "DOCUMENTS"}, std::array<std::string, 3>{"place.music", "Music", "MUSIC"},
                           std::array<std::string, 3>{"place.pictures", "Pictures", "PICTURES"}}) {
        auto path = home / row[1];
        auto found = directories.find("XDG_" + row[2] + "_DIR");
        if(found != directories.end()) {
            auto value = found->second;
            const auto start = value.find_first_not_of(" \t\"");
            const auto end = value.find_last_not_of(" \t\"");
            value = start == std::string::npos ? "" : value.substr(start, end - start + 1);
            path = value.starts_with("$HOME/") ? home / value.substr(6) : std::filesystem::path(value);
        }
        std::error_code error;
        if(path.is_absolute() && path != home && std::filesystem::is_directory(path, error))
            places.emplace_back(i18n::tr(row[0]), path);
    }
    places.emplace_back(i18n::tr("place.filesystem"), "/");
    for(const auto* base : {"/run/media", "/media", "/mnt"}) {
        std::error_code error;
        std::filesystem::directory_iterator children(base, error);
        std::size_t count = 0;
        for(auto end = std::filesystem::directory_iterator{}; !error && children != end && count < 24; children.increment(error), ++count) {
            auto path = children->path();
            std::error_code status_error;
            if(!std::filesystem::is_directory(path, status_error))
                continue;
            auto add = [&](const auto& candidate) {
                std::error_code e;
                if(places.size() < 18 && std::filesystem::is_directory(candidate, e) && std::none_of(places.begin(), places.end(), [&](auto& p) { return p.second == candidate; }))
                    places.emplace_back(candidate.filename().native(), candidate);
            };
            if(path.filename() == home.filename()) {
                std::filesystem::directory_iterator mounts(path, status_error);
                std::size_t scanned = 0;
                for(; !status_error && mounts != end && scanned < 24; mounts.increment(status_error), ++scanned) add(mounts->path());
            } else
                add(path);
        }
    }
    return places;
}
}  // namespace lumaframevr
