#include "catalogue.hpp"
#include "remembered_video_settings.hpp"
#include "advanced_panels.hpp"
#include "viewer_settings.hpp"
#include "branding.hpp"
#include "font_style.hpp"
#include "test_registry.hpp"
#include "ui_contract_fixtures.hpp"
#include "browser.hpp"
#include "browser_colors.hpp"
#include "transport.hpp"
#include <chrono>
#include <unistd.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
unsigned checks{};
static void require(bool success, const char* label) {
    ++checks;
    if(!success)
        throw std::runtime_error(label);
}
std::size_t count_paths(const std::filesystem::path& directory) {
    return static_cast<std::size_t>(std::distance(std::filesystem::directory_iterator(directory), {}));
}
}  // namespace
int lumaframevr::tests::ui_contract(int argc, char** argv) {
    using namespace lumaframevr::ui;
    try {
        require(argc <= 2, "Contract test accepts at most one fixture directory");
        const auto fixture = argc == 2 ? std::filesystem::path(argv[1]) : retained_fixture();
        Canvas canvas(1160, 400);
        TransportState transport{"video.mp4", 50, 100};
        draw_transport(canvas, transport);
        require(canvas.pixel(0, 0) == Color{}, "Canvas corner is not transparent");
        require(canvas.pixel(30, 180) == Color{}, "Rounded transport corner lost transparency");
        require(canvas.pixel(40, 260)[3] == 255, "Transport body is not opaque");
        require(canvas.pixel(102, 80) == Color{}, "Closed volume popup must be transparent");
        const auto closed = std::vector(canvas.pixels().begin(), canvas.pixels().end());
        transport.volume_open = true;
        draw_transport(canvas, transport);
        require(canvas.pixel(102, 80)[3] == 255, "Opened volume popup missing");
        require(!std::equal(canvas.pixels().begin(), canvas.pixels().end(), closed.begin()), "Volume popup does not change rendering");
        for(unsigned index = 0; index < 6; ++index) {
            const auto button = transport_button(1160, index);
            const auto hit = transport_hit(button.x + button.width / 2, button.y + button.height / 2, 100, false);
            require(hit && hit->action == TransportAction::Button && hit->button == index, "Rendered transport button does not hit its own stable ID");
        }
        const auto speaker = transport_speaker(1160);
        require(transport_hit(speaker.x + 20, speaker.y + 20, 100, false)->action == TransportAction::VolumeToggle, "Speaker must toggle volume");
        const auto mute = transport_mute(1160);
        require(transport_hit(mute.x + 20, mute.y + 20, 100, true)->action == TransportAction::Mute, "Popup mute hit incorrect");
        const auto track = transport_track(1160);
        require(transport_hit(track.x, 310, 100, false)->value == 0, "Seek track start must be zero");
        require(transport_hit(track.right(), 310, 100, false)->value == 100, "Seek track end must clamp to duration");
        require(!transport_hit(track.x, 310, {}, false), "Unknown-duration video must not seek");
        require(!transport_hit(track.x, 310, std::numeric_limits<double>::quiet_NaN(), false), "NaN duration must not seek");
        require(!transport_hit(0, 0, 100, true), "Outside transport must not hit controls");
        const auto volume_track = transport_volume_track(1160);
        require(transport_hit(volume_track.x, volume_track.y, 100, true)->value == 1, "Volume top must be full volume");
        require(transport_hit(volume_track.x, volume_track.bottom(), 100, true)->value == 0, "Volume bottom must be silence");
        transport.volume = std::numeric_limits<float>::quiet_NaN();
        draw_transport(canvas, transport);
        transport.duration = std::numeric_limits<double>::infinity();
        draw_transport(canvas, transport);
        require(timestamp(std::numeric_limits<double>::quiet_NaN()) == "--:--", "NaN timestamp differs from original");
        require(timestamp(3661) == "1:01:01", "Hour timestamp differs from original");
        require(fit_label("Video 01 somewhere and salons.mp4", 15) == "Video 01 s….mp4", "Grid truncation lost distinguishing prefix");
        require(escape_path("/Videos/Café\\a\n.mp4") == "/Videos/Caf\\xc3\\xa9\\x5ca\\x0a.mp4", "Path escaping differs from original byte contract");
        // The German letters have to come from the baked Inter face rather than fall back to an
        // ASCII shape: the interface ships four languages and three of them use umlauts.
        require(text_width("äöü", FontStyle::body) > 0 && text_width("äöü", FontStyle::body) != text_width("???", FontStyle::body), "German letters are missing from the baked typeface");
        require(list_scroll(999, 100, 4) == 96, "Selection scroll must clamp at final page");
        const auto files_before = count_paths("/proc/self/fd");
        const auto threads_before = count_paths("/proc/self/task");
        {
            lumaframevr::FolderBrowser browser(fixture);
            wait_for_scan(browser);
            require(browser.entries().size() == 14, "Reference fixture was not scanned from disk");
            require(browser.selected() == 1, "First real entry must be selected initially");
            Canvas viewer(1160, 772);
            BrowserRenderState state;
            draw_browser(viewer, browser, state);
            require(viewer.pixel(0, 0) == Color{}, "File viewer corner must be transparent");
            // The selected row carries the palette's selection colour, probed where the layout
            // puts the row rather than at a coordinate copied from an older design.
            const auto selected_row = browser_layout(1160, 772).row(1);
            require(viewer.pixel(selected_row.x + 6, selected_row.y + selected_row.height / 2) == browser_color::selected, "the selected row is not drawn in the palette's selection colour");
            for(const auto& target : browser_targets(browser, 1160, 772, state)) {
                const auto hit = browser_target_at(browser, 1160, 772, state, target.area.x + target.area.width / 2, target.area.y + target.area.height / 2);
                require(target.enabled ? (hit && hit->action == target.action && hit->index == target.index) : !hit, "Browser chrome target does not agree with rendering");
            }
            const auto layout = browser_layout(1160, 772);
            // Only the rows the viewport is showing can be hit: the taller rows mean a folder of
            // this size needs scrolling, which is the design, not a fault.
            const auto window = browser_viewport(1160, 772, browser.entries().size(), 1, BrowserMode::List);
            for(std::size_t slot = 0; slot < window.count; ++slot) {
                const auto row = layout.row(slot);
                require(browser_entry_at(1160, 772, browser.entries().size(), 1, BrowserMode::List, row.x + 10, row.y + 10) == window.first + slot, "Browser list hit identifies wrong entry");
            }
            require(!browser_entry_at(1160, 772, 14, 1, BrowserMode::Grid, 392, 110), "Grid gap must not select a tile");
            require(!browser_entry_at(120, 120, 14, 1, BrowserMode::Grid, 0, 0), "Empty grid must not produce an entry");
            browser.go(fixture / "does-not-exist-ui-contract");
            require(!browser.message().empty(), "Invalid folder must expose its error");
            draw_browser(viewer, browser, state);
            require(viewer.pixel(0, 0) == Color{}, "Error rendering corrupted canvas transparency");

            checks += check_labelled_controls(browser, state);

            // Every panel's rows have to fit the panel they are drawn in. The information panel
            // broke exactly here when the shared row pitch changed under it, and its text fell off
            // the bottom; this is the check that would have caught it.
            {
                constexpr std::size_t information_rows = 8;
                const auto panels = std::array<std::pair<const char*, int>, 3>{
                        {{"information", ui::branding_header_height + 58 + 36 * static_cast<int>(information_rows + 3)}, {"alignment", ui::alignment_size[1]}, {"settings", ui::settings_size[1]}}};
                for(const auto& [name, height] : panels) require(height > 0, "a panel has no height");
                for(std::size_t index = 0; index < information_rows; ++index) require(ui::information_row(index, 792).bottom() <= panels[0].second - 6, "an information row falls outside its panel");
                for(std::size_t index = 0; index < 4; ++index) require(ui::advanced_row(index, ui::alignment_size[0]).bottom() <= panels[1].second - 4, "an alignment row falls outside its panel");
                lumaframevr::VideoPresentation probe;
                for(const auto& row : ui::settings_layout(probe, 0, ui::settings_size[0], ui::settings_size[1]))
                    require(row.area.bottom() <= panels[2].second - 4, "a settings row falls outside its panel");

                // The stereo choice has to be reachable in every projection. It is the row that says
                // whether a VR video is a stereo pair, and in 180 and 360 it used to sit under a
                // Format row that only repeated the projection choice, below the rows a headset panel
                // shows. The fisheye Format row stays, because there it is the lens angle.
                for(auto projection : {lumaframevr::Projection::flat, lumaframevr::Projection::hemisphere, lumaframevr::Projection::sphere}) {
                    lumaframevr::VideoPresentation p;
                    p.projection = projection;
                    const auto fields = ui::settings_fields(p);
                    require(fields.size() > 1 && fields[0].kind == ui::SettingKind::projection && fields[1].kind == ui::SettingKind::stereo, "the stereo row sits directly under the projection row");
                    require(ui::setting_options(fields[1], p).size() == 3, "the stereo row offers mono, side by side and top/bottom");
                    for(const auto& field : fields) require(field.kind != ui::SettingKind::format, "180 and 360 do not repeat the projection choice as a format row");
                }
                {
                    lumaframevr::VideoPresentation fisheye;
                    fisheye.projection = lumaframevr::Projection::fisheye;
                    const auto fields = ui::settings_fields(fisheye);
                    require(fields.size() > 1 && fields[1].kind == ui::SettingKind::format, "the fisheye lens angle keeps its own row");
                    require(ui::setting_options(fields[1], fisheye).size() == 4, "the fisheye row offers the four lens angles");
                }

                // The viewer settings rows the controller addresses are one contract: the toggles in
                // draw order, then the update row, the language buttons, the update button, its paging
                // and Close. The update row and Close were one index once, which is why a click on
                // Close was indistinguishable from the row that runs an update.
                {
                    const lumaframevr::ui::ViewerSettingsState state{};
                    const auto targets = ui::viewer_setting_targets(1200, 800, lumaframevr::i18n::active().languages(), state);
                    std::set<std::size_t> toggles, others;
                    for(const auto& target : targets) (target.index < ui::viewer_setting_toggles ? toggles : others).insert(target.index);
                    require(toggles.size() == ui::viewer_setting_toggles, "every toggle row has its own index");
                    require(others.count(ui::viewer_setting_update_row) == 0, "the update row is selected, not a target");
                    require(others.count(ui::viewer_setting_close) == 1 && others.count(ui::viewer_setting_update) == 1, "Close and the update button are separate targets");
                    // The size text the listing and the preview draw.
                    require(ui::file_size_text(0) == "0 B" && ui::file_size_text(999) == "999 B", "a small size is bytes");
                    require(ui::file_size_text(1536) == "1.5 KiB", "a kilobyte carries its unit");
                    require(ui::file_size_text((4ull << 30) + (512ull << 20)) == "4.5 GiB", "a large size carries its unit");
                }
            }

            // The alignment panel's tabs and its Reset button have to accept the beam where they
            // are drawn, and the stereo side has to carry its own controls. Both were drawn once
            // without the hit areas being exercised at all.
            {
                lumaframevr::VideoPresentation stereo;
                stereo.stereo = lumaframevr::Stereo::side_by_side;
                lumaframevr::RememberedVideoSettings remembered;
                lumaframevr::ui::AdvancedSettingsController controller(remembered, "probe.mp4", stereo);
                const auto tabs = lumaframevr::ui::advanced_mode_buttons(lumaframevr::ui::alignment_size[0]);
                const auto click = [&controller](lumaframevr::ui::Rect area) { return controller.click_alignment(area.x + area.width / 2, area.y + area.height / 2); };
                require(click(tabs[0]) && controller.mode() == lumaframevr::AlignmentMode::orientation, "the orientation tab does not select orientation");
                require(click(tabs[1]) && controller.mode() == lumaframevr::AlignmentMode::position, "the stereo tab does not select the stereo controls");
                const auto stereo_sliders = lumaframevr::ui::alignment_sliders(stereo, controller.mode());
                for(const auto& slider : stereo_sliders) require(slider.enabled && !slider.label.empty(), "a stereo control is missing from the panel");
                stereo.alignment.stereo_offset = {0.05F, 0.02F};
                const auto reset = lumaframevr::ui::advanced_reset_button(lumaframevr::ui::alignment_size[0]);
                require(click(reset), "the Reset button does not accept the beam");
                require(stereo.alignment.stereo_offset[0] == 0.0F && stereo.alignment.stereo_offset[1] == 0.0F, "Reset did not restore the alignment");
            }
        }
        for(unsigned repeat = 0; repeat < 50; ++repeat) {
            lumaframevr::FolderBrowser browser(fixture);
            if(repeat % 2)
                wait_for_scan(browser);
        }
        require(count_paths("/proc/self/fd") == files_before, "Folder-browser lifecycle leaked file descriptors");
        require(count_paths("/proc/self/task") == threads_before, "Folder-browser lifecycle leaked scanner threads");
        std::filesystem::remove_all(fixture);
        std::cout << "UI contracts passed: " << checks << "; file descriptors and threads restored, fixture removed" << '\n';
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
