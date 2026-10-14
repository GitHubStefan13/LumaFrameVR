#include "media_source.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>

namespace lumaframevr {
MediaSource MediaSource::parse(std::string_view argument) {
    if(argument.find('\0') != std::string_view::npos)
        throw std::invalid_argument("Media source contains NUL");
    const auto separator = argument.find("://");
    if(separator == std::string_view::npos)
        return MediaSource(std::string(argument), false);
    auto scheme = std::string(argument.substr(0, separator));
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    if(scheme != "http" && scheme != "https" && scheme != "ftp")
        throw std::invalid_argument("Unsupported URL scheme '" + scheme + "'. Supported: http, https, ftp");
    return MediaSource(std::string(argument), true);
}
MediaSource MediaSource::resolved() const {
    if(remote_)
        return *this;
    return MediaSource(std::filesystem::canonical(input_).native(), false);
}
std::string MediaSource::file_name() const {
    if(!remote_)
        return std::filesystem::path(input_).filename().native();
    const auto authority = input_.find("://") + 3;
    const auto path = input_.find('/', authority);
    if(path == std::string::npos)
        return input_;
    const auto end = input_.find_first_of("?#", path);
    const auto clean = input_.substr(path, end == std::string::npos ? end : end - path);
    const auto segment = clean.find_last_of('/');
    const auto name = clean.substr(segment + 1);
    return name.empty() ? input_ : name;
}
}  // namespace lumaframevr
