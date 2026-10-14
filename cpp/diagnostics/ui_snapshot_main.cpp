#include "transport.hpp"
#include "browser.hpp"
#include "advanced_panels.hpp"
#include "catalogue.hpp"
#include "viewer_settings.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>
#include <iostream>
#include <stdexcept>

namespace {
// A presentation with every row populated, so a snapshot shows the whole panel instead of the
// handful of rows a plain movie produces.
lumaframevr::VideoPresentation show_every_row() {
    lumaframevr::VideoPresentation p;
    p.projection = lumaframevr::Projection::fisheye;
    p.fisheye_degrees = 190;
    p.stereo = lumaframevr::Stereo::side_by_side;
    p.lens.centers = {{{0.004F, -0.012F}, {0.006F, -0.010F}}};
    p.lens.distortion = {0.021F, -0.004F, 0.001F, 0.0F};
    return p;
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if(argc != 5 || std::string_view(argv[1]) != "--snapshot")
            throw std::invalid_argument("Usage: lumaframevr_ui --snapshot MODE FOLDER OUTPUT.ppm");
        const std::string_view mode(argv[2]);
        if(mode == "folder-list" || mode == "folder-grid") {
            lumaframevr::FolderBrowser browser(argv[3]);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while(browser.loading()) {
                if(std::chrono::steady_clock::now() >= deadline)
                    throw std::runtime_error("Folder scan timed out");
                browser.poll();
                std::this_thread::yield();
            }
            lumaframevr::ui::Canvas canvas(1160, 772);
            lumaframevr::ui::BrowserRenderState state;
            if(mode == "folder-grid")
                state.mode = lumaframevr::ui::BrowserMode::Grid;
            lumaframevr::ui::draw_browser(canvas, browser, state);
            canvas.save_ppm(argv[4]);
            std::cout << "Rendered C++ production file viewer: " << mode << '\n';
            return 0;
        }
        // The settings and alignment panels are only reachable from inside the headset, so a
        // snapshot is the only way to look at them on a development host.
        if(mode == "settings" || mode == "settings-flat") {
            lumaframevr::ui::Canvas canvas(lumaframevr::ui::settings_size[0], lumaframevr::ui::settings_size[1]);
            auto presentation = show_every_row();
            if(mode == "settings-flat") {
                presentation.projection = lumaframevr::Projection::flat;
                presentation.stereo = lumaframevr::Stereo::mono;
            }
            lumaframevr::ui::draw_settings(canvas, presentation, 0, "", false);
            canvas.save_ppm(argv[4]);
            std::cout << "Rendered video settings panel: " << mode << '\n';
            return 0;
        }
        // The viewer settings are only reachable from inside the headset too, and the licence list
        // is read from the checkout so the snapshot shows the real names, not a specimen.
        if(mode == "viewer-settings") {
            lumaframevr::ui::Canvas canvas(1160, 772);
            lumaframevr::ui::ViewerSettingsState state;
            std::error_code error;
            for(const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(argv[3]) / "licenses", error))
                if(entry.is_regular_file())
                    state.licences.push_back(entry.path().filename().string());
            std::sort(state.licences.begin(), state.licences.end());
            lumaframevr::ui::draw_viewer_settings(canvas, state, {{std::string("en"), std::string("English"), std::string("en")}}, "1.0.0");
            canvas.save_ppm(argv[4]);
            std::cout << "Rendered viewer settings panel (" << state.licences.size() << " licences)\n";
            return 0;
        }
        if(mode == "alignment-position" || mode == "alignment-orientation") {
            lumaframevr::ui::Canvas canvas(lumaframevr::ui::alignment_size[0], lumaframevr::ui::alignment_size[1]);
            const auto presentation = show_every_row();
            const auto alignment = mode == "alignment-position" ? lumaframevr::AlignmentMode::position : lumaframevr::AlignmentMode::orientation;
            lumaframevr::ui::draw_alignment(canvas, presentation, alignment, false);
            canvas.save_ppm(argv[4]);
            std::cout << "Rendered alignment panel: " << mode << '\n';
            return 0;
        }
        lumaframevr::ui::Canvas canvas(1160, 400);
        lumaframevr::ui::TransportState state{"video.mp4", 754, 2900, 0.8F, false, true, false, 32};
        if(mode == "volume-open")
            state = {"video.mp4", 50, 100, 0.8F, false, true, true, 1};
        else if(mode != "video-control")
            throw std::invalid_argument("Unknown snapshot mode");
        lumaframevr::ui::draw_transport(canvas, state);
        canvas.save_ppm(argv[4]);
        std::cout << "Rendered C++ production UI: " << mode << '\n';
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
