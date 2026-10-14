#pragma once
// Fixture and checks for the UI contract test, kept here so the test file itself stays small:
// a supporting header is where this repository puts reusable test scaffolding.
#include "browser.hpp"
#include "catalogue.hpp"
#include "font_style.hpp"
#include "test_assertions.hpp"
#include "viewer_settings.hpp"
#include "advanced_panels.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>

namespace lumaframevr::tests {
// A fixture of its own under the temporary directory, removed by the caller: a suite run must not
// leave a directory in the source tree for the project hygiene check to find.
inline std::filesystem::path retained_fixture() {
    const auto parent = std::filesystem::temp_directory_path() / ("lumaframevr-ui-contract-" + std::to_string(getpid()));
    std::filesystem::create_directories(parent);
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = parent / std::to_string(stamp);
    for(unsigned suffix = 0; !std::filesystem::create_directory(path); ++suffix) path = parent / (std::to_string(stamp) + "-" + std::to_string(suffix));
    std::filesystem::create_directory(path / "folder");
    for(unsigned index = 0; index < 12; ++index) {
        std::ofstream file(path / ("clip-" + std::to_string(index) + ".mp4"));
        if(!file)
            throw std::runtime_error("Cannot create retained browser fixture");
    }
    return path;
}

inline void wait_for_scan(lumaframevr::FolderBrowser& browser) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while(browser.loading()) {
        if(std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("Asynchronous folder scan timed out");
        browser.poll();
        std::this_thread::yield();
    }
}

// Every labelled control has to fit the word it draws, in every language the release ships. The
// sizes used to be constants, so the next translation only had to be a little longer than English
// for a button to clip its own label.
inline int check_labelled_controls(const lumaframevr::FolderBrowser& browser, const ui::BrowserRenderState& state) {
    if(!i18n::active().load(i18n::default_directory()))
        return 0;
    const auto carries_icon = [](ui::BrowserAction action) {
        switch(action) {
            case ui::BrowserAction::List:
            case ui::BrowserAction::Grid:
            case ui::BrowserAction::Settings:
            case ui::BrowserAction::Quit:
            case ui::BrowserAction::Sort:
                return true;
            default:
                return false;
        }
    };
    int checks = 0;
    for(const auto& language : i18n::active().languages()) {
        i18n::active().select(language.code);
        for(const auto& target : ui::browser_targets(browser, 1160, 772, state)) {
            // Place rows carry a path label and are truncated on purpose; icon-only controls have
            // nothing to fit.
            if(target.label.empty() || !target.shows_label || target.action == ui::BrowserAction::Place)
                continue;
            const auto needed = static_cast<int>(ui::text_width(target.label, ui::FontStyle::medium)) + (carries_icon(target.action) ? 30 : 0) + 20;
            require(needed <= target.area.width, "a browser control is narrower than its own label");
            ++checks;
        }
        const auto tabs = ui::advanced_mode_buttons(ui::alignment_size[0]);
        const std::array tab_labels{i18n::tr("video.tab.rotation"), i18n::tr("video.tab.position")};
        for(std::size_t index = 0; index < tabs.size(); ++index) {
            require(static_cast<int>(ui::text_width(tab_labels[index], ui::FontStyle::medium)) + 30 <= tabs[index].width, "an alignment tab is narrower than its label");
            ++checks;
        }
        require(static_cast<int>(ui::text_width(i18n::tr("video.row.reset"), ui::FontStyle::medium)) + 28 <= ui::advanced_reset_button(ui::alignment_size[0]).width,
                "the Reset button is narrower than its label");
        ++checks;
        for(const auto& target : ui::viewer_setting_targets(ui::browser_listing_width, 800, i18n::active().languages())) {
            if(target.index >= 3)
                continue;  // the language row holds names and is sized by area
            const auto label = i18n::tr(target.value ? "viewer.on" : "viewer.off");
            require(static_cast<int>(ui::text_width(label, ui::FontStyle::medium)) + 20 <= target.area.width, "a settings toggle is narrower than its label");
            ++checks;
        }
    }
    i18n::active().select("en");
    return checks;
}
}  // namespace lumaframevr::tests
