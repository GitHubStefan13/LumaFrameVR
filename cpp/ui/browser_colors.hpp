#pragma once
#include "canvas.hpp"

namespace lumaframevr::ui::browser_color {
// The file viewer's palette, from the reference: a near black window, a slightly raised sidebar
// and header, one blue for whatever is active, and blue icons for folders.
inline constexpr Color content{20, 20, 20, 255}, places{26, 26, 26, 255}, toolbar{32, 32, 32, 255};
inline constexpr Color band{36, 36, 36, 255}, outline{48, 48, 48, 255}, highlight{44, 44, 44, 255};
inline constexpr Color text{237, 237, 237, 255}, dim{158, 158, 158, 255};
inline constexpr Color selected{38, 86, 148, 255}, accent{53, 132, 228, 255};
inline constexpr Color folder{78, 154, 224, 255}, folder_back{58, 110, 178, 255}, paper{226, 228, 233, 255};
}  // namespace lumaframevr::ui::browser_color
