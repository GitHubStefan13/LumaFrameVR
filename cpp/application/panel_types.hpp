#pragma once
#include <array>
namespace lumaframevr::application {
enum class PanelKind { browser, information, transport, settings, rotation, startup };
struct Hit {
    PanelKind panel;
    int width{}, height{}, x{}, y{};
    std::array<float, 2> point{};
    unsigned slot{};
    bool operator==(const Hit&) const = default;
};
}  // namespace lumaframevr::application
