#include "test_registry.hpp"
#include "application_controller.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace lumaframevr;
using namespace lumaframevr::application;
static void require(bool value, const char* message) {
    if(!value)
        throw std::runtime_error(message);
}
int lumaframevr::tests::routing(int argc, char** argv) {
    try {
        require(argc == 2, "fixture directory required");
        const std::filesystem::path fixture(argv[1]);
        std::filesystem::create_directories(fixture / "folder");
        std::ofstream(fixture / "a.mp4", std::ios::app);
        std::ofstream(fixture / "b.txt", std::ios::app);
        ApplicationOptions options;
        options.file = fixture;
        Controller app(options);
        for(unsigned tries = 0; app.explorer.browser.loading() && tries < 1000; ++tries) {
            app.update({}, {}, tries * .002);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(!app.explorer.browser.loading(), "real asynchronous scan completed");
        require(app.explorer.browser.entries().size() == 4, "all files and directories retained");
        require(app.explorer.browser.entries()[0].directory, "directories first");
        require(app.panels()[2] == PanelKind::browser, "startup browser");
        app.select_entry(2);
        require(app.queued() == fixture / "a.mp4", "exact clicked file queued without stepping");
        app.synchronize([] {});
        app.update({}, {}, 1);
        require(app.playing(), "nonblocking worker opened");
        ControllerState grips;
        grips.grips = {true, true};
        app.update(grips, {}, 2);
        require(app.panels()[0] == PanelKind::browser && app.panels()[1] == PanelKind::browser, "both grips browser");
        ControllerState rotation;
        rotation.pressed[0] = true;
        app.update(rotation, {}, 3);
        require(app.dialogs.shown(ui::AdvancedKind::rotation), "X opens persistent rotation");
        ControllerState back;
        back.pressed[2] = true;
        app.update(back, {}, 4);
        require(app.playing() && !app.dialogs.shown(ui::AdvancedKind::rotation), "B closes dialog before stopping");
        app.update(back, {}, 5);
        require(!app.playing(), "B stops playback");
        app.synchronize([] {});
        require(app.retiring(), "decoder shutdown kept off frame thread");
        std::cout << "PASS production controller routing and lifecycle\n";
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
