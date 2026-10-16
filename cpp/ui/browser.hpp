#pragma once
#include "canvas.hpp"
#include "folder_browser.hpp"
#include <optional>

namespace lumaframevr::ui {
inline constexpr int browser_listing_width = 1200;
enum class BrowserMode { List, Grid };
enum class BrowserAction { Back, Forward, Up, Refresh, Sort, Hidden, Place, Breadcrumb, List, Grid, Passthrough, Settings, Quit };
struct BrowserTarget {
    Rect area;
    BrowserAction action;
    std::size_t index{};
    std::string label;
    bool enabled{true}, active{};
    // Whether this control draws its label or only its icon. The test that every control fits its
    // own label needs to know, and guessing from the width is what let a clipped one through.
    bool shows_label{true};
};
struct Thumbnail {
    std::size_t index{};
    std::vector<std::uint8_t> pixels;
    int width{}, height{};
};
struct BrowserRenderState {
    BrowserMode mode{BrowserMode::List};
    std::optional<std::size_t> hover;
    std::span<const Thumbnail> thumbnails;
    bool stopping{}, descending{}, show_hidden{};
    bool passthrough_enabled{}, passthrough_supported{};
    bool viewer_settings{};
    // Off by default: a listing is a list of names until someone asks for sizes, and the viewer
    // settings panel carries that switch.
    bool show_file_sizes{};
};
struct BrowserLayout {
    Rect card, rows, footer;
    std::size_t capacity() const;
    Rect row(std::size_t slot) const;
};
struct BrowserViewport {
    BrowserLayout layout;
    BrowserMode mode;
    std::size_t first{}, count{};
    int columns{}, rows{};
    Rect rectangle(std::size_t slot) const;
    std::optional<std::size_t> entry_at(int x, int y) const;
};
BrowserLayout browser_layout(int width, int height);
BrowserViewport browser_viewport(int width, int height, std::size_t count, std::size_t selected, BrowserMode mode);
std::size_t list_scroll(std::size_t selected, std::size_t count, std::size_t rows);
std::vector<std::filesystem::path> browser_ancestors(const std::filesystem::path& directory);
std::vector<BrowserTarget> browser_targets(const FolderBrowser& browser, int width, int height, const BrowserRenderState& state);
std::optional<BrowserTarget> browser_target_at(const FolderBrowser& browser, int width, int height, const BrowserRenderState& state, int x, int y);
std::optional<std::size_t> browser_entry_at(int width, int height, std::size_t count, std::size_t selected, BrowserMode mode, int x, int y);
void draw_browser(Canvas& canvas, const FolderBrowser& browser, const BrowserRenderState& state = {});
void draw_browser_chrome(Canvas& canvas, const FolderBrowser& browser, const BrowserRenderState& state);
// A file size for a listing or a preview: binary units with IEC names, which is what the rest of the
// interface uses for memory, and one decimal place so two similar files can be told apart.
std::string file_size_text(std::uintmax_t bytes);
void draw_file_icon(Canvas& canvas, Rect area, bool directory, bool video);
bool is_video_path(const std::filesystem::path& path);
void draw_browser_entries(Canvas& canvas, const FolderBrowser& browser, const BrowserRenderState& state);
}  // namespace lumaframevr::ui
