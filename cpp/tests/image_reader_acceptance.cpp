#include "test_registry.hpp"
#include "image_reader.hpp"
#include "test_assertions.hpp"
#include <array>
#include <filesystem>
#include <sys/resource.h>
int lumaframevr::tests::image_reader(int argc, char** argv) {
    return acceptance_main([&] {
        using namespace lumaframevr;
        if(argc != 2 || !std::filesystem::is_directory(argv[1]))
            skip("LUMAFRAMEVR_IMAGE_FIXTURES is not set");
        const std::filesystem::path root(argv[1]);
        rusage before{}, after{};
        getrusage(RUSAGE_SELF, &before);
        rejects([&] { read_image(root / "too-large.png"); }, "pixel budget rejected during metadata probing");
        getrusage(RUSAGE_SELF, &after);
        require(after.ru_maxrss - before.ru_maxrss < 128 * 1024, "metadata probe must not materialize an over-budget PNG");
        for(const char* extension : {"png", "bmp", "jpg", "webp", "tiff", "gif"}) {
            const auto image = read_image(root / (std::string("corners.") + extension));
            require(image.width == 96 && image.height == 64 && image.rgba.size() == 96 * 64 * 4, "image dimensions and packed RGBA");
            const std::array<std::array<unsigned char, 4>, 4> expected{{{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}, {255, 255, 255, 255}}};
            for(unsigned corner = 0; corner < 4; ++corner) {
                const auto offset = ((corner >= 2 ? 56 : 8) * 96 + (corner % 2 ? 80 : 8)) * 4;
                for(unsigned c = 0; c < 4; ++c)
                    require(std::abs(static_cast<int>(image.rgba[offset + c]) - expected[corner][c]) <= (std::string_view(extension) == "jpg" ? 3 : 0),
                            "asymmetric decoded corners preserve orientation/color");
            }
        }
        for(unsigned orientation = 1; orientation <= 8; ++orientation) {
            const auto stem = "exif" + std::to_string(orientation);
            const auto actual = read_image(root / (stem + ".jpg"));
            const auto expected = read_image(root / (stem + "-expected.png"));
            require(actual.width == expected.width && actual.height == expected.height, "EXIF orientation dimensions");
            for(std::size_t index = 0; index < actual.rgba.size(); ++index)
                require(std::abs(static_cast<int>(actual.rgba[index]) - expected.rgba[index]) <= 3, "EXIF rotations and reflections follow embedded metadata");
        }
        const auto gray16 = read_image(root / "gray16.png");
        require(gray16.rgba[0] == 120 && gray16.rgba[1] == 120 && gray16.rgba[2] == 120, "16-bit gray normalized correctly");
        rejects([&] { read_image(root / "broken.png"); }, "corrupt image rejected");
        rejects([&] { read_image(root / "huge.png"); }, "oversize image rejected before materialization");
        const auto alpha = read_image(root / "alpha.png");
        require(alpha.rgba[(3 * 96 + 3) * 4 + 3] == 64, "PNG alpha retained");
        const auto odd = read_image(root / "odd.png");
        require(odd.width == 19 && odd.height == 13, "odd image dimensions retained");
        const auto gray = read_image(root / "gray.png");
        require(gray.rgba[0] == 120 && gray.rgba[1] == 120 && gray.rgba[2] == 120, "gray converted to RGB");
        const auto palette = read_image(root / "palette.png");
        require(palette.rgba[0] == 255 && palette.rgba[1] == 0 && palette.rgba[2] == 0, "palette red is not byte-order swapped");
        rejects([&] { read_image(root / "missing.png"); }, "missing image rejected");
        rejects([&] { read_image(root); }, "directory is not an image");
        rejects([&] { read_image(root / "corners.png", [] { return true; }); }, "cancelled image rejected");
        std::cout << "PNG/JPEG/BMP/WebP/TIFF/GIF first frame, alpha, grayscale, palette, odd size and cancellation verified\n";
    });
}
