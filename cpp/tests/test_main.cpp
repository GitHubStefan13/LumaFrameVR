// Dispatcher for the single lumaframevr_tests binary.
//
// Usage: lumaframevr_tests <test> [arguments...]
// Run with no arguments to list the available tests.
#include "test_registry.hpp"
#include <iostream>
#include <string_view>

namespace {
struct Entry {
    std::string_view name;
    int (*run)(int, char**);
};

constexpr Entry entries[] = {
        {"application_options", lumaframevr::tests::application_options},
        {"browser", lumaframevr::tests::browser},
        {"browser_viewport", lumaframevr::tests::browser_viewport},
        {"catalogue", lumaframevr::tests::catalogue},
        {"controller", lumaframevr::tests::controller},
        {"controller_bindings", lumaframevr::tests::controller_bindings},
        {"dma_import", lumaframevr::tests::dma_import},
        {"gpu_color", lumaframevr::tests::gpu_color},
        {"image_loader", lumaframevr::tests::image_loader},
        {"image_reader", lumaframevr::tests::image_reader},
        {"localization", lumaframevr::tests::localization},
        {"media", lumaframevr::tests::media},
        {"media_errors", lumaframevr::tests::media_errors},
        {"audio_native", lumaframevr::tests::audio_native},
        {"audio_operations", lumaframevr::tests::audio_operations},
        {"packet_reader", lumaframevr::tests::packet_reader},
        {"packet_limits", lumaframevr::tests::packet_limits},
        {"seek_preroll", lumaframevr::tests::seek_preroll},
        {"presentation_native", lumaframevr::tests::presentation_native},
        {"v4l2_formats", lumaframevr::tests::v4l2_formats},
        {"v4l2_layout", lumaframevr::tests::v4l2_layout},
        {"v4l2_queue", lumaframevr::tests::v4l2_queue},
        {"v4l2_device", lumaframevr::tests::v4l2_device},
        {"v4l2_bitstream", lumaframevr::tests::v4l2_bitstream},
        {"media_loop", lumaframevr::tests::media_loop},
        {"presentation", lumaframevr::tests::presentation},
        {"routing", lumaframevr::tests::routing},
        {"shader_module", lumaframevr::tests::shader_module},
        {"source", lumaframevr::tests::source},
        {"timeline", lumaframevr::tests::timeline},
        {"ui_contract", lumaframevr::tests::ui_contract},
};
}  // namespace

int main(int argc, char** argv) {
    if(argc < 2) {
        std::cout << "usage: lumaframevr_tests <test> [arguments...]\n\ntests:\n";
        for(const auto& entry : entries) std::cout << "  " << entry.name << '\n';
        return 2;
    }
    const std::string_view requested = argv[1];
    for(const auto& entry : entries) {
        if(entry.name == requested)
            return entry.run(argc - 1, argv + 1);
    }
    std::cerr << "unknown test: " << requested << '\n';
    return 2;
}
