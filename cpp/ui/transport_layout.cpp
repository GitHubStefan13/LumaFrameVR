#include "transport.hpp"
#include <algorithm>
#include <cmath>

namespace lumaframevr::ui {
namespace {
Rect row_button(int width, float fraction) {
    const auto card = transport_card(width);
    return {card.x + static_cast<int>(std::round(card.width * fraction)) - 28, card.y + 42, 56, 56};
}
}  // namespace
Rect transport_card(int width) {
    return {30, 180, width - 60, 164};
}
Rect transport_button(int width, unsigned index) {
    constexpr std::array fractions{0.40F, 0.50F, 0.60F, 0.80F, 0.88F, 0.14F};
    return row_button(width, fractions.at(index));
}
Rect transport_speaker(int width) {
    return row_button(width, 0.065F);
}
Rect transport_passthrough(int width) {
    return row_button(width, 0.70F);
}
Rect transport_popup(int width) {
    return {transport_speaker(width).x - 20, 20, 96, 188};
}
Rect transport_volume_track(int width) {
    return {transport_popup(width).x + 45, 38, 6, 120};
}
Rect transport_mute(int width) {
    return {transport_popup(width).x + 28, 164, 40, 40};
}
Rect transport_track(int width) {
    const auto card = transport_card(width);
    const int begin = card.x + static_cast<int>(std::round(card.width * 0.17F));
    const int end = card.x + static_cast<int>(std::round(card.width * 0.83F));
    return {begin, 307, end - begin, 6};
}
std::optional<TransportHit> transport_hit(int x, int y, std::optional<double> duration, bool volume_open, bool still_image) {
    constexpr int width = 1160;
    if(!Rect{18, 18, 1124, 364}.contains(x, y))
        return {};
    if(!still_image && volume_open && transport_popup(width).contains(x, y)) {
        if(transport_mute(width).contains(x, y))
            return TransportHit{TransportAction::Mute};
        const auto track = transport_volume_track(width);
        if(std::abs(x - track.x - track.width / 2) <= 24 && y >= track.y - 10 && y < track.bottom() + 10)
            return TransportHit{TransportAction::Volume, 0, std::clamp(1.0F - static_cast<float>(y - track.y) / (track.height - 1), 0.0F, 1.0F)};
        return {};
    }
    if(!still_image && transport_speaker(width).contains(x, y))
        return TransportHit{TransportAction::VolumeToggle};
    if(transport_passthrough(width).contains(x, y))
        return TransportHit{TransportAction::Passthrough};
    if(row_button(width, 0.215F).contains(x, y))
        return TransportHit{TransportAction::Minimize};
    if(row_button(width, 0.955F).contains(x, y))
        return TransportHit{TransportAction::Move};
    for(unsigned index = 0; index < 6; ++index)
        if(transport_button(width, index).contains(x, y))
            return TransportHit{TransportAction::Button, index};
    const auto track = transport_track(width);
    if(x >= track.x && x <= track.right() && std::abs(y - 310) <= 20 && !still_image && duration && std::isfinite(*duration) && *duration > 0)
        return TransportHit{TransportAction::Seek, 0, std::clamp(static_cast<double>(x - track.x) / (track.width - 1), 0.0, 1.0) * *duration};
    return {};
}
}  // namespace lumaframevr::ui
