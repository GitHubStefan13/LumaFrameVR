#include "application_scene.hpp"
#include "branding.hpp"
#include "catalogue.hpp"
#include "font_style.hpp"
#include <algorithm>
namespace lumaframevr::application {
namespace {
void cursor(ui::Canvas& canvas, int x, int y) {
    constexpr int radius = 6, inner = 4;
    for(int dy = -radius; dy <= radius; ++dy)
        for(int dx = -radius; dx <= radius; ++dx) {
            const int distance = dx * dx + dy * dy;
            if(distance >= inner * inner && distance <= radius * radius)
                canvas.put(x + dx, y + dy, {170, 170, 170, 255});
        }
}
}  // namespace
ui::Canvas draw_panel(const PanelSnapshot& app, const ScenePanel& panel, double now) {
    ui::Canvas canvas(panel.pixels[0], panel.pixels[1]);
    if(panel.kind == PanelKind::browser) {
        ui::Canvas listing(ui::browser_listing_width, panel.pixels[1]);
        std::vector<ui::Thumbnail> thumbnails;
        for(const auto& shown : app.thumbnails) thumbnails.push_back({shown.index, shown.tile->pixels, shown.tile->width, shown.tile->height});
        ui::BrowserRenderState state;
        state.mode = app.view;
        state.thumbnails = thumbnails;
        state.stopping = app.retiring;
        state.passthrough_enabled = app.passthrough_enabled;
        state.passthrough_supported = app.passthrough_supported;
        state.viewer_settings = app.viewer_settings.open;
        for(const auto& hit : app.hover)
            if(hit && hit->panel == PanelKind::browser)
                state.hover = ui::browser_entry_at(1200, panel.pixels[1], app.browser.entries().size(), app.browser.selected(), app.view, hit->x, hit->y);
        ui::draw_browser(listing, app.browser, state);
        if(app.viewer_settings.open)
            ui::draw_viewer_settings(listing, app.viewer_settings, i18n::active().languages(), LUMAFRAMEVR_VERSION);
        if(app.language_prompt.open)
            ui::draw_language_prompt(listing, app.language_prompt, i18n::active().languages());
        canvas.blit(listing);
        canvas.rounded({1200, 6, 314, panel.pixels[1] - 12}, 10, {35, 35, 35, 255});
        draw_text(canvas, 1216, 24, i18n::tr("preview.title"), {220, 220, 220, 255}, ui::FontStyle::medium);
        draw_text(canvas, 1216, 226, ui::fit_label(app.browser.selected_path().filename().string(), 23), {180, 180, 180, 255}, ui::FontStyle::secondary);
        if(app.preview_details && app.preview_details->path == app.browser.selected_path()) {
            const auto& detail = *app.preview_details;
            draw_text(canvas, 1216, 258, std::to_string(detail.width) + " x " + std::to_string(detail.height), {180, 180, 180, 255}, ui::FontStyle::secondary);
            if(detail.duration)
                draw_text(canvas, 1216, 286, i18n::tr("preview.duration", {{"time", ui::timestamp(*detail.duration)}}), {180, 180, 180, 255}, ui::FontStyle::secondary);
        }
        if(!app.error.empty())
            draw_text(canvas, 216, panel.pixels[1] - 30, ui::fit_label(app.error, 78), {255, 160, 130, 255}, ui::FontStyle::secondary);
    } else if(panel.kind == PanelKind::startup) {
        ui::draw_branding(canvas, {0, 0, canvas.width(), canvas.height()}, false);
    } else if(panel.kind == PanelKind::transport) {
        auto state = app.transport;
        ui::draw_transport(canvas, state);
    } else if(panel.kind == PanelKind::settings) {
        ui::draw_settings(canvas, app.presentation, app.selected_setting, "", true);
    } else if(panel.kind == PanelKind::rotation) {
        ui::draw_alignment(canvas, app.presentation, app.alignment_mode, true);
    } else {
        const auto state = app.transport;
        ui::Canvas information(canvas.width(), canvas.height() - ui::branding_header_height);
        if(state.still_image) {
            draw_text(information, 40, 32, i18n::tr("info.still_image"), {230, 230, 230, 255}, ui::FontStyle::medium);
            draw_text(information, 40, 64, ui::fit_label(app.path.filename().string(), 60), {180, 180, 180, 255}, ui::FontStyle::secondary);
            const auto status = state.image_loading ? i18n::tr("transport.loading_image")
                                                    : i18n::tr("transport.image", {{"width", std::to_string(state.image_width)}, {"height", std::to_string(state.image_height)}});
            draw_text(information, 40, 96, status, {180, 180, 180, 255}, ui::FontStyle::secondary);
        } else
            diagnostics::draw_information(information, app.path, app.telemetry, app.device_sample, now, app.xr_fps, app.target_fps, 0, state.playing ? "Playing" : "Paused", state.position,
                                          state.duration);
        canvas.blit(information, 0, ui::branding_header_height);
        ui::draw_branding(canvas, {0, 0, canvas.width(), ui::branding_header_height}, true);
    }
    for(const auto& hit : app.hover)
        if(hit && hit->panel == panel.kind && hit->slot == panel.slot)
            cursor(canvas, hit->x, hit->y);
    return canvas;
}
}  // namespace lumaframevr::application
