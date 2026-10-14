#include "branding.hpp"
#include "branding_assets.hpp"
#include <algorithm>
namespace lumaframevr::ui {
namespace {
void image(Canvas& canvas, const BrandingImage& source, Rect bounds) {
    canvas.image(bounds, {reinterpret_cast<const std::uint8_t*>(source.pixels.data()), source.pixels.size() * sizeof(Color)}, source.width, source.height);
}
}  // namespace
void draw_branding(Canvas& canvas, Rect bounds, bool compact) {
    canvas.fill(bounds, {0, 0, 0, 255});
    const auto& images = branding_images();
    if(compact) {
        image(canvas, images.icon, {bounds.x + 16, bounds.y + 12, 96, 96});
        image(canvas, images.logo, {bounds.x + 124, bounds.y + 6, 208, 108});
    } else {
        const int padding = bounds.width / 16, icon_width = bounds.width / 5;
        image(canvas, images.icon, {bounds.x + padding, bounds.y, icon_width, bounds.height});
        image(canvas, images.logo, {bounds.x + padding * 2 + icon_width, bounds.y + bounds.height / 10, bounds.width - padding * 3 - icon_width, bounds.height * 4 / 5});
    }
}
}  // namespace lumaframevr::ui
