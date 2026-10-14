#include "test_registry.hpp"
#include "catalogue.hpp"
#include "test_assertions.hpp"
#include "persistent_test_fixtures.hpp"
#include <algorithm>
#include <fstream>
#include <set>
using namespace lumaframevr;

namespace {
std::set<std::string> placeholders(std::string_view value) {
    std::set<std::string> result;
    for(auto open = value.find('{'); open != value.npos; open = value.find('{', open + 1)) {
        const auto close = value.find('}', open);
        if(close == value.npos)
            break;
        result.insert(std::string(value.substr(open + 1, close - open - 1)));
    }
    return result;
}

void write(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << text;
}
}  // namespace

int lumaframevr::tests::catalogue(int argc, char** argv) {
    return acceptance_main([&] {
        require(argc == 3, "language directory and owned fixture directory required");
        const std::filesystem::path directory(argv[1]);
        const auto root = persistent_test_fixture(argv[2]);
        std::filesystem::create_directories(root);
        i18n::Catalogue catalogue;
        require(catalogue.load(directory), "shipped language files load");
        require(catalogue.languages().size() >= 4, "English, Spanish, French and Italian are all discovered from files");
        std::set<std::string> codes;
        for(const auto& language : catalogue.languages()) {
            codes.insert(language.code);
            require(!language.name.empty() && language.file.extension() == ".lang", "every language carries a name and a file");
        }
        for(const auto* code : {"en", "es", "fr", "it"}) require(codes.count(code) == 1, std::string("discovered language: ") + code);
        const auto english = catalogue.entries("en");
        require(english.size() > 100, "the English catalogue covers the whole interface");
        for(const auto& [key, value] : english) {
            static_cast<void>(value);
            if(key == "code" || key == "name")
                continue;
            for(const auto& language : catalogue.languages()) {
                if(language.code == "en")
                    continue;
                const auto translated = catalogue.entries(language.code);
                require(translated.count(key) == 1, language.code + " translates " + key);
                require(!translated.at(key).empty(), language.code + " keeps a value for " + key);
                require(placeholders(translated.at(key)) == placeholders(english.at(key)), language.code + " keeps every placeholder for " + key);
            }
        }
        require(!catalogue.select("zz"), "an unknown language code is refused");
        require(catalogue.select("fr"), "a discovered language can be selected");
        require(catalogue.active() == "fr" && catalogue.name_of("fr").size() > 0, "the active language and its name follow selection");
        require(catalogue.text("browser.settings") == catalogue.entries("fr").at("browser.settings"), "text is read from the active language");
        const auto formatted = catalogue.format("browser.items", {{"count", "7"}});
        require(formatted.find("{count}") == formatted.npos && formatted.find('7') != formatted.npos, "placeholders are substituted");
        require(catalogue.text("missing.key") == "missing.key", "a key without a translation reports the key itself");
        i18n::Catalogue partial;
        write(root / "lang/de.lang", "code = de\nname = Deutsch\nbrowser.settings = Einstellungen\n");
        write(root / "lang/en.lang", "code = en\nname = English\nbrowser.settings = Settings\nbrowser.folders = Folders\n");
        require(partial.load(root / "lang"), "a partial language set loads");
        require(partial.select("de") && partial.text("browser.folders") == "Folders", "a missing key falls back to English");
        require(partial.format("browser.items", {{"count", "2"}}) == "browser.items", "an untranslated key stays visible instead of being invented");
        write(root / "broken/duplicate.lang", "code = du\nname = Duplicate\nbrowser.settings = One\nbrowser.settings = Two\n");
        i18n::Catalogue broken;
        rejects([&] { broken.load(root / "broken"); }, "a duplicate key is rejected");
        write(root / "nameless/nameless.lang", "code = nl\nbrowser.settings = Naamloos\n");
        i18n::Catalogue nameless;
        rejects([&] { nameless.load(root / "nameless"); }, "a language file without a name is rejected");
        write(root / "junk/junk.lang", "code = jk\nname = Junk\nnot a statement\n");
        i18n::Catalogue junk;
        rejects([&] { junk.load(root / "junk"); }, "a malformed line is rejected");
        require(!i18n::Catalogue().load(root / "absent"), "a missing language directory reports failure instead of guessing");
    });
}
