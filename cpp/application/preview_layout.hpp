#pragma once
#include "panel_types.hpp"
#include "pose_math.hpp"
#include <optional>
namespace lumaframevr::application {
inline constexpr std::array<int, 2> preview_extent{256, 144};
struct PreviewInputs {
    bool explorer{}, previous_explorer{}, selection_changed{}, transport_visible{}, shown{};
    double position{};
    std::optional<double> duration;
};
struct PreviewPanel {
    PanelKind kind;
    std::array<int, 2> pixels;
    std::array<float, 2> size;
};
struct PreviewPlacement {
    Pose offset;
    float width{}, height{};
};
struct PreviewLayout {
    bool clear{}, visible{};
    std::optional<PreviewPlacement> placement;
};
PreviewLayout preview_layout(PreviewInputs input, std::optional<PreviewPanel> panel = {});
}  // namespace lumaframevr::application
