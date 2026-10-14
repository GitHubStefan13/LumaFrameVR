#include "test_registry.hpp"
#include "application_controller.hpp"
#include "test_assertions.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
int lumaframevr::tests::browser_viewport(int argc, char** argv) {
    return acceptance_main([&] {
        using namespace lumaframevr;
        require(argc == 2, "fixture directory required");
        const std::filesystem::path path(argv[1]);
        std::filesystem::create_directories(path);
        for(unsigned i = 0; i < 5000; ++i) std::ofstream(path / (std::to_string(i) + ".mp4"), std::ios::app);
        ApplicationOptions options;
        options.file = path;
        application::Controller app(options);
        const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while(app.explorer.browser.loading() && std::chrono::steady_clock::now() < limit) {
            app.explorer.browser.poll();
            std::this_thread::yield();
        }
        require(!app.explorer.browser.loading(), "scan completed");
        for(const auto mode : {ui::BrowserMode::List, ui::BrowserMode::Grid}) {
            app.explorer.view = mode;
            for(auto selected : {1U, 4000U, 5000U}) {
                app.explorer.browser.select(selected);
                const auto started = std::chrono::steady_clock::now();
                const auto wanted = app.wanted_tiles();
                const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
                std::cout << "wanted visible=" << wanted.entries.size() << " milliseconds=" << ms << '\n';
                require(ms < 30, "visibility selection must not scan hidden files with pixel probes");
                require(wanted.entries.size() <= (mode == ui::BrowserMode::List ? 18U : 25U), "visible capacity respected");
                require(!wanted.entries.empty(), "visible videos requested");
                // Probe the cell the viewport itself reports, rather than coordinates copied
                // from whatever the layout happened to be when this test was written: the file
                // viewer's rows are the viewer's business.
                const auto probe = ui::browser_viewport(1200, 800, app.explorer.browser.entries().size(), app.explorer.browser.selected(), mode);
                for(const auto& [index, file] : wanted.entries) {
                    require(file == app.explorer.browser.entries()[index].path, "index/path association retained");
                    require(index >= probe.first && index - probe.first < probe.count, "requested tile belongs to displayed viewport");
                    const auto rect = probe.rectangle(index - probe.first);
                    const auto hit = ui::browser_entry_at(1200, 800, app.explorer.browser.entries().size(), app.explorer.browser.selected(), mode, rect.x + rect.width / 2, rect.y + rect.height / 2);
                    require(hit && *hit == index, "a displayed entry receives the beam where it is drawn");
                }
            }
        }
    });
}
