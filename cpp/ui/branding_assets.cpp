#include "branding_assets.hpp"
#include <array>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
namespace lumaframevr::ui {
namespace {
std::filesystem::path directory() {
    if(const auto* override = std::getenv("LUMAFRAMEVR_BRANDING_DIR"))
        return override;
    if(const auto* package = std::getenv("LUMAFRAMEVR_PACKAGE_ROOT"))
        return std::filesystem::path(package) / "branding";
    const auto executable = std::filesystem::read_symlink("/proc/self/exe").parent_path();
    for(const auto& path : {executable / "branding", executable.parent_path() / "share/lumaframevr/branding"})
        if(std::filesystem::is_regular_file(path / "icon.rgba") && std::filesystem::is_regular_file(path / "logo.rgba"))
            return path;
    throw std::runtime_error("LumaFrameVR branding assets missing beside executable or installed share directory");
}
}  // namespace
BrandingImage read_branding_image(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::array<std::uint8_t, 8> header{};
    if(!file.read(reinterpret_cast<char*>(header.data()), header.size()))
        throw std::runtime_error("LumaFrameVR cannot read branding header: " + path.string());
    const auto dimension = [&](unsigned offset) {
        std::uint32_t value{};
        for(unsigned index = 0; index < 4; ++index) value |= static_cast<std::uint32_t>(header[offset + index]) << (index * 8);
        if(value == 0 || value > 2048)
            throw std::runtime_error("LumaFrameVR invalid branding dimensions: " + path.string());
        return static_cast<int>(value);
    };
    BrandingImage image{dimension(0), dimension(4), {}};
    image.pixels.resize(static_cast<std::size_t>(image.width) * image.height);
    const auto bytes = static_cast<std::streamsize>(image.pixels.size() * sizeof(Color));
    if(!file.read(reinterpret_cast<char*>(image.pixels.data()), bytes) || file.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("LumaFrameVR invalid branding pixel payload: " + path.string());
    return image;
}
const BrandingImages& branding_images() {
    static const auto images = [] {
        const auto root = directory();
        return BrandingImages{read_branding_image(root / "icon.rgba"), read_branding_image(root / "logo.rgba")};
    }();
    return images;
}
}  // namespace lumaframevr::ui
