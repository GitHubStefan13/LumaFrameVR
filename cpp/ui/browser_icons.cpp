#include "browser.hpp"
#include "browser_colors.hpp"
#include "icons.hpp"
#include "media_file_type.hpp"

namespace lumaframevr::ui {
void draw_file_icon(Canvas& canvas, Rect area, bool directory, bool video) {
    using namespace browser_color;
    // Lucide marks: a folder for a folder, a player for a video, a picture for everything else.
    if(directory)
        draw_icon(canvas, area, "folder", folder);
    else
        draw_icon(canvas, area, video ? "video" : "image", text);
}
bool is_video_path(const std::filesystem::path& path) {
    return media_file_type(path) == MediaFileType::video;
}
}  // namespace lumaframevr::ui
