#include "viewer_settings.hpp"
#include "font_style.hpp"
#include "catalogue.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#if __has_include("update_process.hpp")
#include "update_process.hpp"
#include "application_controller.hpp"
#define HAS_UPDATER 1
#endif

namespace {
void require(bool condition, const char* message) {
    if(!condition)
        throw std::runtime_error(message);
}
#ifdef HAS_UPDATER
void wait(lumaframevr::application::UpdateProcess& process) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while(process.state().status == lumaframevr::ui::UpdateStatus::checking) {
        require(std::chrono::steady_clock::now() < deadline, "updater fixture timed out");
        process.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void check(const std::filesystem::path& root) {
    namespace ui = lumaframevr::ui;
    namespace app = lumaframevr::application;
    app::UpdateProcess process;
    require(process.state().status == ui::UpdateStatus::never_checked, "updater starts never checked");
    const auto script = root / "fixture.sh";
    {
        std::ofstream output(script);
        output << "#!/bin/sh\nprintf 'Checking fixture\\n'\nsleep 0.1\nprintf 'AVAILABLE\\t1.10.0\\tfixture library changes\\n'\n";
    }
    process.start(script, false);
    require(process.state().status == ui::UpdateStatus::checking, "script starts asynchronously");
    wait(process);
    require(process.state().status == ui::UpdateStatus::available && process.state().version == "1.10.0", "exit zero and available protocol expose new version");
    require(process.state().message == "fixture library changes", "library changes survive the script boundary");
    {
        std::ofstream output(script);
        output << "#!/bin/sh\nprintf 'latest\\n'\nexit 3\n";
    }
    process.start(script, false);
    wait(process);
    require(process.state().status == ui::UpdateStatus::up_to_date, "exit three is up to date, not failure");
    {
        std::ofstream output(script);
        output << "#!/bin/sh\nprintf 'first\\nTLS fixture failure without newline' >&2\nexit 1\n";
    }
    process.start(script, false);
    wait(process);
    require(process.state().status == ui::UpdateStatus::failed && process.state().message == "TLS fixture failure without newline", "failure is the script's last line verbatim");
    {
        std::ofstream output(script);
        output << "#!/bin/sh\nprintf 'installed\\n'\nexit 0\n";
    }
    process.start(script, true);
    wait(process);
    require(process.state().status == ui::UpdateStatus::installed, "successful silent installation signals clean player exit");
    process.start(root / "absent.sh", false);
    wait(process);
    require(process.state().status == ui::UpdateStatus::failed && !process.state().message.empty(), "missing script fails with a reason");

    lumaframevr::i18n::active().load(lumaframevr::i18n::default_directory());
    ui::UpdateState state;
    state.status = ui::UpdateStatus::failed;
    state.message = "short failure";
    const auto short_row = ui::update_row_layout({0, 0, 732, 0}, state, 0);
    state.message = std::string(250, 'W');
    const auto long_row = ui::update_row_layout({0, 0, 732, 0}, state, 0);
    require(long_row.area.height > short_row.area.height, "update row grows with measured failure text");
    state.message = std::string(2000, 'W');
    const auto paged = ui::update_row_layout({0, 0, 732, 0}, state, 0);
    require(paged.pages > 1 && paged.more.width > 0, "long failure remains reachable through detail pages");
    std::string joined;
    for(std::size_t page = 0; page < paged.pages; ++page) {
        const auto row = ui::update_row_layout({0, 0, 732, 0}, state, page);
        for(const auto& line : row.lines) {
            require(ui::text_width(line, ui::FontStyle::secondary) <= 732, "even an unbroken version or failure cannot clip horizontally");
            joined += line;
        }
        require(row.area.height <= 180, "bounded update row cannot overlap licences and footer");
    }
    require(joined == state.message, "paging preserves every character of the failure reason");
    for(const auto& language : lumaframevr::i18n::active().languages()) {
        lumaframevr::i18n::active().select(language.code);
        state.status = ui::UpdateStatus::available;
        state.version = std::string(256, '9');
        const auto row = ui::update_row_layout({0, 0, 732, 0}, state, 0);
        for(const auto& label : row.button_lines) require(ui::text_width(label, ui::FontStyle::medium) + 24 <= row.button.width, "translated update button measures the text it draws");
        for(const auto& line : row.lines) require(ui::text_width(line, ui::FontStyle::secondary) <= 732, "long available version wraps in every language");
    }
    lumaframevr::i18n::active().select("en");
    ui::ViewerSettingsState settings;
    settings.update = state;
    settings.licences = {"fixture-license"};
    ui::Canvas canvas(800, 800);
    ui::draw_viewer_settings(canvas, settings, lumaframevr::i18n::active().languages(), "1.0.0");
    canvas.save_ppm((root / "updater-long-status.ppm").string());
    const auto targets = ui::viewer_setting_targets(800, 800, lumaframevr::i18n::active().languages(), settings);
    for(const auto& target : targets)
        if(target.index >= 5) {
            const auto hit = ui::viewer_setting_at(800, 800, lumaframevr::i18n::active().languages(), target.area.x + 1, target.area.y + 1, settings);
            require(hit && hit->index == target.index, "drawing and hit testing use the same measured update rectangles");
        }

    const auto previous = std::getenv("LUMAFRAMEVR_PACKAGE_ROOT");
    const std::optional<std::string> previous_root = previous ? std::optional<std::string>(previous) : std::nullopt;
    setenv("LUMAFRAMEVR_PACKAGE_ROOT", root.c_str(), 1);
    {
        std::ofstream output(root / "update.sh");
        output << "#!/bin/sh\nif [ \"$1\" = --check ]; then printf 'AVAILABLE\\t9.0.0\\tfixture notes\\n'; else printf 'installed\\n'; fi\n";
    }
    lumaframevr::ApplicationOptions options;
    options.file = root;
    app::Controller controller(options);
    controller.browser_action({{}, ui::BrowserAction::Settings, 0, {}, true, false});
    const auto click_update = [&] {
        for(const auto& target : ui::viewer_setting_targets(800, 800, lumaframevr::i18n::active().languages(), controller.viewer_settings()))
            if(target.index == ui::viewer_setting_update)
                controller.activate({app::PanelKind::browser, 800, 800, target.area.x + 1, target.area.y + 1, 0}, 0);
    };
    const auto poll_controller = [&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while(controller.viewer_settings().update.status == ui::UpdateStatus::checking) {
            require(std::chrono::steady_clock::now() < deadline, "controller update fixture timed out");
            controller.update({}, {}, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };
    click_update();
    require(controller.viewer_settings().update.status == ui::UpdateStatus::checking, "real settings hit starts the packaged script");
    poll_controller();
    require(controller.viewer_settings().update.status == ui::UpdateStatus::available, "controller publishes available state to the panel");
    click_update();
    poll_controller();
    require(controller.exit_requested(), "successful update through the settings row requests clean player exit");
    if(previous_root)
        setenv("LUMAFRAMEVR_PACKAGE_ROOT", previous_root->c_str(), 1);
    else
        unsetenv("LUMAFRAMEVR_PACKAGE_ROOT");
}
#endif
}  // namespace
int main(int argc, char** argv) {
    try {
#ifndef HAS_UPDATER
        require(false, "update process and measured row are not implemented");
#else
        require(argc == 2, "updater acceptance requires a fixture directory");
        std::filesystem::create_directories(argv[1]);
        check(argv[1]);
#endif
        (void)argc;
        (void)argv;
        std::cout << "updater fixture: asynchronous process states, verbatim errors and measured paged geometry verified without network\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
