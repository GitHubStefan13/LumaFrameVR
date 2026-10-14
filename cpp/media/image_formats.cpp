#include "image_reader.hpp"
#include "media_file_type.hpp"
namespace lumaframevr {
bool is_image_path(const std::filesystem::path& path) {
    return media_file_type(path) == MediaFileType::image;
}
}  // namespace lumaframevr
