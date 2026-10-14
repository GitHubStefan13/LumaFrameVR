#include "canvas.hpp"
#include <algorithm>
#include <fstream>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace lumaframevr::ui {
bool Rect::contains(int px, int py) const {
    return px >= x && px < right() && py >= y && py < bottom();
}
Rect Rect::inset(int amount) const {
    return {x + amount, y + amount, std::max(0, width - amount * 2), std::max(0, height - amount * 2)};
}
Canvas::Canvas(int width, int height) : width_(width), height_(height) {
    if(width <= 0 || height <= 0 || width > 16384 || height > 16384)
        throw std::invalid_argument("Invalid UI canvas dimensions");
    pixels_.resize(static_cast<std::size_t>(width) * height * 4);
}
void Canvas::blit(const Canvas& source, int x, int y) {
    if(x >= width_ || y >= height_ || x <= -source.width_ || y <= -source.height_)
        return;
    if(this == &source) {
        const auto copy = source;
        blit(copy, x, y);
        return;
    }
    const int left = std::max(0, -x), top = std::max(0, -y);
    const int right = std::min(source.width_, width_ - x), bottom = std::min(source.height_, height_ - y);
    const auto bytes = static_cast<std::size_t>(right - left) * 4;
    for(int row = top; row < bottom; ++row) {
        const auto from = (static_cast<std::size_t>(source.height_ - 1 - row) * source.width_ + left) * 4;
        const auto to = (static_cast<std::size_t>(height_ - 1 - row - y) * width_ + left + x) * 4;
        std::memcpy(pixels_.data() + to, source.pixels_.data() + from, bytes);
    }
}
void Canvas::clear() {
    std::fill(pixels_.begin(), pixels_.end(), 0);
}
void Canvas::put(int x, int y, Color color) {
    if(x < 0 || y < 0 || x >= width_ || y >= height_)
        return;
    const auto offset = (static_cast<std::size_t>(height_ - 1 - y) * width_ + x) * 4;
    std::copy(color.begin(), color.end(), pixels_.begin() + static_cast<std::ptrdiff_t>(offset));
}
Color Canvas::pixel(int x, int y) const {
    if(x < 0 || y < 0 || x >= width_ || y >= height_)
        throw std::out_of_range("UI pixel outside canvas");
    const auto offset = (static_cast<std::size_t>(height_ - 1 - y) * width_ + x) * 4;
    return {pixels_[offset], pixels_[offset + 1], pixels_[offset + 2], pixels_[offset + 3]};
}
void Canvas::fill(Rect area, Color color) {
    if(!color[3])
        return;
    for(int y = std::max(0, area.y); y < std::min(height_, area.bottom()); ++y)
        for(int x = std::max(0, area.x); x < std::min(width_, area.right()); ++x) put(x, y, color);
}
void Canvas::rounded(Rect area, int radius, Color color) {
    if(area.width <= 0 || area.height <= 0)
        return;
    radius = std::clamp(radius, 0, std::min(area.width, area.height) / 2);
    if(!radius) {
        fill(area, color);
        return;
    }
    for(int y = std::max(0, area.y); y < std::min(height_, area.bottom()); ++y) {
        const auto dy = std::min(y - area.y, area.bottom() - 1 - y);
        for(int x = std::max(0, area.x); x < std::min(width_, area.right()); ++x) {
            const auto dx = std::min(x - area.x, area.right() - 1 - x);
            if(dx < radius && dy < radius) {
                const auto cx = radius - dx, cy = radius - dy;
                if(cx * cx + cy * cy > radius * radius)
                    continue;
            }
            put(x, y, color);
        }
    }
}
void Canvas::outline(Rect area, int radius, int thickness, Color color) {
    for(int i = 0; i < std::max(1, thickness); ++i) {
        const auto ring = area.inset(i);
        if(ring.width <= 0 || ring.height <= 0)
            break;
        const auto rounding = std::max(0, radius - i);
        rounded({ring.x, ring.y, ring.width, 1}, rounding, color);
        rounded({ring.x, ring.bottom() - 1, ring.width, 1}, rounding, color);
        rounded({ring.x, ring.y, 1, ring.height}, rounding, color);
        rounded({ring.right() - 1, ring.y, 1, ring.height}, rounding, color);
    }
}
void Canvas::save_ppm(const std::filesystem::path& path) const {
    std::ofstream output(path, std::ios::binary);
    if(!output)
        throw std::runtime_error("Cannot write UI snapshot: " + path.string());
    output << "P6\n" << width_ << ' ' << height_ << "\n255\n";
    for(int y = 0; y < height_; ++y)
        for(int x = 0; x < width_; ++x) {
            const auto color = pixel(x, y);
            output.write(reinterpret_cast<const char*>(color.data()), 3);
        }
    if(!output)
        throw std::runtime_error("Failed writing UI snapshot: " + path.string());
}
}  // namespace lumaframevr::ui
