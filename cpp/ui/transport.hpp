#pragma once
#include "canvas.hpp"
#include <optional>

namespace lumaframevr::ui {
struct TransportState {
    std::string title;
    double position{};
    std::optional<double> duration;
    float volume{0.8F};
    bool muted{}, playing{true}, volume_open{};
    unsigned speed{1};
    bool passthrough_enabled{}, passthrough_supported{};
    bool still_image{}, image_loading{};
    int image_width{}, image_height{};
};
enum class TransportAction { Button, Seek, Volume, VolumeToggle, Mute, Move, Minimize, Passthrough };
struct TransportHit {
    TransportAction action;
    unsigned button{};
    double value{};
};
Rect transport_card(int width);
Rect transport_button(int width, unsigned index);
Rect transport_speaker(int width);
Rect transport_passthrough(int width);
Rect transport_popup(int width);
Rect transport_volume_track(int width);
Rect transport_mute(int width);
Rect transport_track(int width);
void draw_transport(Canvas& canvas, const TransportState& state);
std::optional<TransportHit> transport_hit(int x, int y, std::optional<double> duration, bool volume_open, bool still_image = false);
}  // namespace lumaframevr::ui
