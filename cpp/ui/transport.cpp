#include "transport.hpp"
#include "transport_assets.hpp"
#include "catalogue.hpp"
#include "font_style.hpp"
#include "assets/passthrough_eye.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace lumaframevr::ui {
void draw_transport(Canvas& canvas, const TransportState& state) {
    constexpr Color white{255, 255, 255, 255}, icons{230, 230, 230, 255};
    constexpr Color gray{170, 170, 170, 255}, unplayed{83, 83, 83, 255}, played{224, 188, 112, 255};
    canvas.clear();
    const auto card = transport_card(canvas.width());
    canvas.rounded(card, 24, {22, 22, 22, 255});
    const std::array<std::span<const std::uint8_t, 1600>, 6> glyphs{assets::transport_previous, state.playing ? assets::transport_pause : assets::transport_play,
                                                                    assets::transport_next,     assets::transport_settings,
                                                                    assets::transport_rotation, assets::transport_information};
    for(unsigned index = 0; index < glyphs.size(); ++index) {
        const auto button = transport_button(canvas.width(), index);
        if(state.still_image && index == 1)
            draw_text(canvas, button.x + 4, button.y + 20, fit_label(i18n::tr("transport.back"), static_cast<std::size_t>(std::max(1, button.width - 4) / 12)), icons, FontStyle::medium);
        else
            canvas.glyph(button, glyphs[index], icons);
    }
    if(!state.still_image)
        canvas.glyph(transport_speaker(canvas.width()), state.muted ? assets::transport_mute : assets::transport_speaker, icons);
    const auto eye = transport_passthrough(canvas.width());
    if(state.passthrough_supported && state.passthrough_enabled)
        canvas.rounded(eye, 8, {40, 62, 94, 255});
    canvas.glyph(eye, assets::passthrough_eye, state.passthrough_supported ? icons : Color{92, 92, 92, 255});
    const auto row = [&](float fraction) { return Rect{card.x + static_cast<int>(std::round(card.width * fraction)) - 28, 222, 56, 56}; };
    canvas.glyph(row(0.215F), assets::transport_minimize, icons);
    canvas.glyph(row(0.955F), assets::transport_move, icons);
    const auto title = fit_label(state.title, static_cast<std::size_t>((card.width - 40) / 12), false);
    draw_text(canvas, card.x + (card.width - text_width(title, FontStyle::medium)) / 2, 194, title, gray, FontStyle::medium);
    if(state.still_image) {
        const auto status =
                state.image_loading ? i18n::tr("transport.loading_image") : i18n::tr("transport.image", {{"width", std::to_string(state.image_width)}, {"height", std::to_string(state.image_height)}});
        draw_text(canvas, card.x + 40, 300, status, gray, FontStyle::medium);
        return;
    }
    const auto track = transport_track(canvas.width());
    float fraction = 0;
    if(state.duration && *state.duration > 0)
        fraction = static_cast<float>(std::clamp(state.position / *state.duration, 0.0, 1.0));
    canvas.rounded(track, 3, unplayed);
    const int filled = std::isfinite(fraction) ? static_cast<int>(std::round(track.width * fraction)) : 0;
    if(filled > 0)
        canvas.rounded({track.x, track.y, std::min(filled, track.width), track.height}, 3, played);
    if(state.duration && *state.duration > 0) {
        const auto marker = track.x + (std::isfinite(fraction) ? static_cast<int>(std::round(fraction * std::max(1, track.width - 1))) : 0);
        canvas.rounded({marker - 11, 299, 22, 22}, 11, white);
    }
    auto elapsed = timestamp(state.position);
    if(state.speed > 1) {
        elapsed.push_back(' ');
        elapsed += std::to_string(state.speed);
        elapsed.push_back('x');
    }
    draw_text(canvas, card.x + 40, 300, elapsed, gray, FontStyle::medium);
    const auto total = state.duration ? timestamp(*state.duration) : i18n::tr("transport.time_unknown");
    draw_text(canvas, card.right() - 40 - text_width(total, FontStyle::medium), 300, total, gray, FontStyle::medium);
    if(state.volume_open) {
        canvas.rounded(transport_popup(canvas.width()), 18, {30, 30, 32, 255});
        const auto volume = transport_volume_track(canvas.width());
        canvas.rounded(volume, 3, unplayed);
        const auto level = state.muted ? 0.0F : std::clamp(state.volume, 0.0F, 1.0F);
        const auto offset = (1.0F - level) * (volume.height - 1);
        const auto handle = volume.y + (std::isfinite(offset) ? static_cast<int>(std::round(offset)) : 0);
        canvas.rounded({volume.x, handle, volume.width, volume.bottom() - handle}, 3, played);
        canvas.rounded({volume.x + volume.width / 2 - 11, handle - 11, 22, 22}, 11, white);
        canvas.glyph(transport_mute(canvas.width()), state.muted ? assets::transport_mute : assets::transport_speaker, icons);
    }
}
}  // namespace lumaframevr::ui
