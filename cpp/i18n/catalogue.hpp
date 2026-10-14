#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lumaframevr::i18n {
struct Language {
    std::string code;
    std::string name;
    std::filesystem::path file;
};

class Catalogue {
   public:
    bool load(const std::filesystem::path& directory);
    bool select(std::string_view code);
    const std::vector<Language>& languages() const { return languages_; }
    const std::string& active() const { return active_; }
    std::string name_of(std::string_view code) const;
    bool covers(std::string_view code) const;
    std::string text(std::string_view key) const;
    std::string format(std::string_view key, const std::vector<std::pair<std::string, std::string>>& values) const;
    std::vector<std::string> keys() const;
    std::map<std::string, std::string> entries(std::string_view code) const;

   private:
    void adopt(std::string_view code, std::string_view name, const std::filesystem::path& file, std::map<std::string, std::string> strings);
    std::map<std::string, std::map<std::string, std::string>> strings_;
    std::vector<Language> languages_;
    std::string active_{"en"};
};

Catalogue& active();
std::string tr(std::string_view key);
std::string tr(std::string_view key, const std::vector<std::pair<std::string, std::string>>& values);
std::filesystem::path default_directory();
}  // namespace lumaframevr::i18n
