#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lumaframevr::ui {
using Color = std::array<std::uint8_t, 4>;
struct Rect {
    int x{}, y{}, width{}, height{};
    int right() const { return x + width; }
    int bottom() const { return y + height; }
    bool contains(int px, int py) const;
    Rect inset(int amount) const;
};
class Canvas {
   public:
    Canvas(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }
    std::span<const std::uint8_t> pixels() const { return pixels_; }
    void clear();
    void blit(const Canvas& source, int x = 0, int y = 0);
    void put(int x, int y, Color color);
    Color pixel(int x, int y) const;
    void fill(Rect area, Color color);
    void rounded(Rect area, int radius, Color color);
    void outline(Rect area, int radius, int thickness, Color color);
    void image(Rect area, std::span<const std::uint8_t> rgba, int width, int height, std::uint8_t opacity = 255);
    void glyph(Rect area, std::span<const std::uint8_t, 1600> coverage, Color color);
    // Draws coverage of any size into an area, resampling it. The icons are baked at their native
    // 24 pixels and drawn at whatever size a control needs.
    void glyph(Rect area, std::span<const std::uint8_t> coverage, int source_width, int source_height, Color color);
    // Blends a coverage mask at its own size, which is how the baked glyphs are drawn.
    void blend(int x, int y, std::span<const std::uint8_t> coverage, int width, int height, Color color);
    void save_ppm(const std::filesystem::path& path) const;

   private:
    int width_, height_;
    std::vector<std::uint8_t> pixels_;
};
std::u32string decode_utf8(std::string_view text);
std::string encode_utf8(std::u32string_view text);
std::string fit_label(std::string_view text, std::size_t columns, bool preserve_extension = true);
std::string escape_path(const std::filesystem::path& path);
std::string timestamp(double seconds);
}  // namespace lumaframevr::ui
