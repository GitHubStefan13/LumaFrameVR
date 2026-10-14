#pragma once
#include "canvas.hpp"
#include <filesystem>
namespace lumaframevr::ui {
struct BrandingImage {
    int width{}, height{};
    std::vector<Color> pixels;
};
struct BrandingImages {
    BrandingImage icon, logo;
};
BrandingImage read_branding_image(const std::filesystem::path& path);
const BrandingImages& branding_images();
}  // namespace lumaframevr::ui
