#include "test_registry.hpp"
#include "vulkan_compute_kernel.hpp"
#include "test_assertions.hpp"
#include <array>
#include <cstring>

int lumaframevr::tests::gpu_color(int, char**) {
    return acceptance_main([] {
        using namespace lumaframevr;
        auto device = VulkanDevice::create();
        VulkanHostBuffer results(device, 5 * sizeof(std::array<float, 4>));
        std::memset(results.bytes().data(), 0, results.bytes().size());
        VulkanComputeKernel kernel(device, std::string(LUMAFRAMEVR_SHADER_DIRECTORY) + "/video_color_check.comp.spv");
        VulkanSubmission submission(device);
        kernel.dispatch(submission.command(), results);
        submission.submit();
        submission.wait();
        std::array<std::array<float, 4>, 5> pixels{};
        std::memcpy(pixels.data(), results.bytes().data(), sizeof(pixels));
        for(const auto& pixel : pixels) {
            for(std::size_t i = 0; i < 3; ++i) require(std::isfinite(pixel[i]) && pixel[i] >= 0 && pixel[i] <= 1, "GPU tone mapped RGB finite and clamped");
            near(pixel[3], 1, 1e-6, "shader dispatch really executed");
        }
        near(pixels[0][0], 0, 1e-6, "PQ black");
        near(pixels[1][0], 0.505, 0.015, "PQ 100 nit maps to nominal SDR reference");
        near(pixels[2][0], 1, 0.015, "PQ 1000 nit white");
        near(pixels[3][0], 1, 0.015, "HLG peak white");
        near(pixels[4][0], 0.214041, 0.001, "retained SDR sRGB inverse transfer");
        std::cout << "GPU=" << device->name() << "\n";
        for(std::size_t i = 0; i < pixels.size(); ++i) std::cout << "color[" << i << "]=" << pixels[i][0] << ',' << pixels[i][1] << ',' << pixels[i][2] << '\n';
        rejects([&] { VulkanHostBuffer invalid(device, 0); }, "empty buffer rejected");
        rejects([&] { VulkanComputeKernel invalid(device, "/nonexistent/shader.spv"); }, "shader load failure cleans up");
        std::cout << "real GPU PQ/HLG/SDR dispatch, queue fence and resource lifetime verified\n";
    });
}
