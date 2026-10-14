#include "test_registry.hpp"
#include "application_controller.hpp"
#include "test_assertions.hpp"
#include "persistent_test_fixtures.hpp"
#include <algorithm>
#include <fstream>
using namespace lumaframevr;
using namespace lumaframevr::application;

int lumaframevr::tests::localization(int argc, char** argv) {
    return acceptance_main([&] {
        require(argc == 3, "language directory and owned fixture directory required");
        require(i18n::active().load(argv[1]), "shipped language directory loads");
        const auto root = persistent_test_fixture(argv[2]);
        const auto media = root / "media";
        std::filesystem::create_directories(media);
        const auto state = root / "first/viewer.state";
        ApplicationOptions options;
        options.file = media;
        options.viewer_state = state;
        Controller first(options);
        require(!first.language_prompt(), "a constructed controller does not decide the first start by itself");
        first.begin_first_start();
        require(first.language_prompt(), "the actual first start of the application asks for a language");
        require(i18n::active().active() == "en", "English is the default language before any choice");
        const auto& languages = i18n::active().languages();
        require(languages.size() >= 4, "the first-start prompt offers every discovered language");
        const auto card = ui::language_prompt_card(ui::browser_listing_width, 800);
        const auto buttons = ui::language_buttons(languages, {card.x + 24, card.y + 96, card.width - 48, 34});
        require(buttons.size() == languages.size(), "every language has its own direct button");
        require(ui::language_prompt_at(languages, ui::browser_listing_width, 800, buttons.front().area.x + 2, buttons.front().area.y + 2).has_value(), "the prompt exposes real pointer targets");
        const auto spanish = std::find_if(buttons.begin(), buttons.end(), [](const auto& button) { return button.code == "es"; });
        require(spanish != buttons.end(), "Spanish is offered on the first start");
        first.activate({PanelKind::browser, ui::browser_listing_width, 800, spanish->area.x + 2, spanish->area.y + 2, {}}, 1);
        require(!first.language_prompt(), "choosing a language closes the first-start prompt");
        require(i18n::active().active() == "es" && first.viewer_settings().language == "es", "the chosen language becomes active immediately");
        Controller second(options);
        second.begin_first_start();
        require(!second.language_prompt(), "a later start does not ask for the language again");
        require(i18n::active().active() == "es" && second.viewer_settings().language == "es", "the remembered language is restored");
        const auto settings = ui::viewer_setting_targets(ui::browser_listing_width, 800, languages);
        const auto offered = std::count_if(settings.begin(), settings.end(), [](const auto& target) { return target.index == 4; });
        require(offered == static_cast<int>(languages.size()), "advanced settings offers every language directly");
        const auto french = std::find_if(settings.begin(), settings.end(), [](const auto& target) { return target.language == "fr"; });
        require(french != settings.end(), "French has its own settings target");
        ui::BrowserRenderState render;
        const auto toolbar = ui::browser_targets(second.explorer.browser, ui::browser_listing_width, 800, render);
        const auto gear = std::find_if(toolbar.begin(), toolbar.end(), [](const auto& target) { return target.action == ui::BrowserAction::Settings; });
        require(gear != toolbar.end(), "advanced settings has a toolbar entry");
        second.activate({PanelKind::browser, ui::browser_listing_width, 800, gear->area.x + 4, gear->area.y + 4, {}}, 1);
        require(second.viewer_settings().open, "the settings modal opens through its own target");
        second.activate({PanelKind::browser, ui::browser_listing_width, 800, french->area.x + 2, french->area.y + 2, {}}, 2);
        require(i18n::active().active() == "fr", "the settings selection switches the interface language");
        Controller third(options);
        third.begin_first_start();
        require(!third.language_prompt() && i18n::active().active() == "fr", "a settings selection persists into the next start");
        const auto clip = root / "first start clip.mp4";
        std::ofstream(clip) << "not a real video";
        ApplicationOptions direct_options;
        direct_options.file = clip;
        direct_options.viewer_state = root / "direct/viewer.state";
        Controller direct(direct_options);
        direct.begin_first_start();
        require(!direct.language_prompt(), "a first start that opens media directly does not prompt over a hidden browser");
        require(i18n::active().select("en"), "the interface can be switched back to English");
        ui::Canvas english_canvas(1200, 800);
        ui::draw_browser_chrome(english_canvas, third.explorer.browser, render);
        require(i18n::active().select("es"), "the interface can be switched to Spanish again");
        ui::Canvas spanish_canvas(1200, 800);
        ui::draw_browser_chrome(spanish_canvas, third.explorer.browser, render);
        require(!std::equal(english_canvas.pixels().begin(), english_canvas.pixels().end(), spanish_canvas.pixels().begin()), "the drawn interface text follows the selected language");
        i18n::active().select("en");
        const auto targets = ui::browser_targets(third.explorer.browser, 1200, 800, render);
        for(const auto& target : targets)
            if(target.action == ui::BrowserAction::Settings)
                require(target.label == i18n::tr("browser.settings"), "toolbar labels are read from the catalogue, not from literals");
    });
}
