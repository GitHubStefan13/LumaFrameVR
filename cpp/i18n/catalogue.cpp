#include "catalogue.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace lumaframevr::i18n {
namespace {
bool valid_key(std::string_view key) {
    if(key.empty() || key.size() > 96)
        return false;
    for(const auto character : key)
        if(!((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '.' || character == '_' || character == '-'))
            return false;
    return true;
}

std::string trimmed(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r");
    if(first == value.npos)
        return {};
    const auto last = value.find_last_not_of(" \t\r");
    return std::string(value.substr(first, last - first + 1));
}

std::string unescaped(std::string_view value) {
    std::string result;
    for(std::size_t index = 0; index < value.size(); ++index) {
        if(value[index] != '\\' || index + 1 == value.size()) {
            result.push_back(value[index]);
            continue;
        }
        const auto next = value[++index];
        if(next == 'n')
            result.push_back('\n');
        else if(next == 't')
            result.push_back('\t');
        else
            result.push_back(next);
    }
    return result;
}

std::map<std::string, std::string> parse(const std::filesystem::path& file) {
    std::ifstream input(file);
    if(!input)
        throw std::runtime_error("Language file is unreadable: " + file.string());
    std::map<std::string, std::string> strings;
    std::string line;
    std::size_t number = 0;
    while(std::getline(input, line)) {
        ++number;
        const auto statement = trimmed(line);
        if(statement.empty() || statement.front() == '#' || statement.front() == ';')
            continue;
        const auto separator = statement.find('=');
        if(separator == statement.npos)
            throw std::runtime_error(file.filename().string() + ":" + std::to_string(number) + ": expected key = value");
        const auto key = trimmed(statement.substr(0, separator));
        if(!valid_key(key))
            throw std::runtime_error(file.filename().string() + ":" + std::to_string(number) + ": invalid key " + key);
        if(strings.count(key))
            throw std::runtime_error(file.filename().string() + ":" + std::to_string(number) + ": duplicate key " + key);
        strings.emplace(key, unescaped(trimmed(statement.substr(separator + 1))));
    }
    if(!strings.count("code") || !strings.count("name"))
        throw std::runtime_error(file.filename().string() + ": language file needs a code and a name");
    if(strings.at("code").size() > 16)
        throw std::runtime_error(file.filename().string() + ": language code is too long");
    return strings;
}
}  // namespace

std::filesystem::path default_directory() {
    if(const char* override = std::getenv("LUMAFRAMEVR_LANG_DIR"))
        return override;
    if(const char* root = std::getenv("LUMAFRAMEVR_PACKAGE_ROOT"))
        return std::filesystem::path(root) / "lang";
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if(!error) {
        const auto beside = executable.parent_path() / "lang";
        if(std::filesystem::is_directory(beside))
            return beside;
        return std::filesystem::path(LUMAFRAMEVR_LANG_SOURCE_DIR);
    }
    return std::filesystem::path(LUMAFRAMEVR_LANG_SOURCE_DIR);
}

void Catalogue::adopt(std::string_view code, std::string_view name, const std::filesystem::path& file, std::map<std::string, std::string> strings) {
    languages_.push_back({std::string(code), std::string(name), file});
    strings_[std::string(code)] = std::move(strings);
}

bool Catalogue::load(const std::filesystem::path& directory) {
    strings_.clear();
    languages_.clear();
    if(!std::filesystem::is_directory(directory))
        return false;
    std::vector<std::filesystem::path> files;
    for(const auto& entry : std::filesystem::directory_iterator(directory))
        if(entry.is_regular_file() && entry.path().extension() == ".lang")
            files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    std::set<std::string> seen;
    for(const auto& file : files) {
        auto strings = parse(file);
        const auto code = strings.at("code");
        const auto name = strings.at("name");
        if(!seen.insert(code).second)
            continue;
        adopt(code, name, file, std::move(strings));
    }
    if(!strings_.count(active_))
        active_ = languages_.empty() ? std::string{"en"} : languages_.front().code;
    return !languages_.empty();
}

bool Catalogue::select(std::string_view code) {
    if(!strings_.count(std::string(code)))
        return false;
    active_ = code;
    return true;
}

bool Catalogue::covers(std::string_view code) const {
    return strings_.count(std::string(code)) > 0;
}

std::string Catalogue::name_of(std::string_view code) const {
    for(const auto& language : languages_)
        if(language.code == code)
            return language.name;
    return std::string(code);
}

std::string Catalogue::text(std::string_view key) const {
    const auto translated = strings_.find(active_);
    if(translated != strings_.end()) {
        const auto value = translated->second.find(std::string(key));
        if(value != translated->second.end())
            return value->second;
    }
    const auto fallback = strings_.find("en");
    if(fallback != strings_.end()) {
        const auto value = fallback->second.find(std::string(key));
        if(value != fallback->second.end())
            return value->second;
    }
    return std::string(key);
}

std::string Catalogue::format(std::string_view key, const std::vector<std::pair<std::string, std::string>>& values) const {
    auto result = text(key);
    for(const auto& [name, value] : values) {
        const auto token = "{" + name + "}";
        for(auto found = result.find(token); found != result.npos; found = result.find(token, found + value.size())) result.replace(found, token.size(), value);
    }
    return result;
}

std::vector<std::string> Catalogue::keys() const {
    std::vector<std::string> result;
    const auto english = entries("en");
    for(const auto& [key, value] : english) {
        static_cast<void>(value);
        if(key != "code" && key != "name")
            result.push_back(key);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::map<std::string, std::string> Catalogue::entries(std::string_view code) const {
    const auto found = strings_.find(std::string(code));
    return found == strings_.end() ? std::map<std::string, std::string>{} : found->second;
}

Catalogue& active() {
    static Catalogue catalogue;
    static const bool loaded = catalogue.load(default_directory());
    static_cast<void>(loaded);
    return catalogue;
}

std::string tr(std::string_view key) {
    return active().text(key);
}

std::string tr(std::string_view key, const std::vector<std::pair<std::string, std::string>>& values) {
    return active().format(key, values);
}
}  // namespace lumaframevr::i18n
