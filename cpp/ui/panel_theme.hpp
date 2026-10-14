#pragma once
#include "canvas.hpp"

namespace lumaframevr::ui {
// The panel palette, taken from the reference design: a near black surface, grey containers for
// anything inactive, and one vivid green for whatever is selected or filled.
struct PanelTheme {
    static constexpr Color surface{14, 14, 14, 255};
    static constexpr Color surface_raised{28, 28, 28, 255};
    static constexpr Color surface_container{42, 42, 42, 255};
    static constexpr Color outline{64, 64, 64, 255};
    static constexpr Color on_surface{237, 237, 237, 255};
    static constexpr Color on_surface_variant{176, 176, 176, 255};
    static constexpr Color primary{46, 232, 74, 255};
    static constexpr Color on_primary{10, 26, 8, 255};
    static constexpr Color primary_text{75, 232, 106, 255};
    static constexpr Color track_inactive{58, 58, 58, 255};
    static constexpr Color thumb_fill{255, 255, 255, 255};
};
}  // namespace lumaframevr::ui
