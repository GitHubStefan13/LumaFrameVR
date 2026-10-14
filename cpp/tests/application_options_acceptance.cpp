#include "test_registry.hpp"
#include "application_options.hpp"
#include "test_assertions.hpp"
#include <initializer_list>
#include <string_view>
#include <vector>

int lumaframevr::tests::application_options(int, char**) {
    return acceptance_main([] {
        using namespace lumaframevr;
        const auto parse = [](std::initializer_list<std::string_view> values) {
            const std::vector<std::string_view> arguments(values);
            return parse_application_options(arguments);
        };
        const auto options = parse({".", "--projection", "flat", "--stereo", "mono", "--sbs-format", "half", "--tb-format", "half", "--hud", "--stats", "--seconds", "+1.5"});
        require(options.action == ApplicationAction::run, "Normal command must run the application");
        require(options.presentation.projection == Projection::flat, "Explicit flat projection lost");
        require(options.presentation.stereo == Stereo::mono, "Explicit mono stereo lost");
        require(options.presentation.sbs_format == StereoFormat::half, "Explicit SBS format lost");
        require(options.presentation.tb_format == StereoFormat::half, "Explicit TB format lost");
        require(options.hud && options.stats && options.seconds == 1.5, "Runtime flags lost");
        require(options.overrides.projection.value.has_value() && options.overrides.stereo.value.has_value(), "Explicit defaults must remain overrides");
        const auto implicit = parse({"."});
        require(!implicit.overrides.projection.value && !implicit.overrides.stereo.value, "Missing flags must remain unknown hints");
        require(parse({"--version", "--invalid"}).action == ApplicationAction::version, "Version exits immediately, as the original does");
        require(parse({"--help"}).action == ApplicationAction::help, "Help action lost");
        for(const auto seconds : {"0", "-1", "NaN", "inf", "1junk", " 1"}) rejects([&] { parse({".", "--seconds", seconds}); }, "Invalid seconds must be rejected");
        rejects([&] { parse({".", "--projection", "unknown"}); }, "Unknown projection accepted");
        rejects([&] { parse({".", "--stereo"}); }, "Missing stereo accepted");
        rejects([&] { parse({".", "--probe"}); }, "Probe accepted a directory");
        rejects([&] { parse({".", "."}); }, "Multiple positional sources accepted");
        rejects([&] { parse({".", "--unknown"}); }, "Unknown option accepted");
        const auto remote = parse({"https://example.org/video.mp4", "--probe", "--snapshot", "frame.ppm"});
        require(remote.source && remote.source->remote() && remote.file.empty(), "URL must not become a local path");
        require(remote.probe && remote.snapshot == "frame.ppm", "Probe and snapshot flags lost");
        rejects([&] { parse({"https://example.org/video.mp4", "--probe", "--render-probe"}); }, "Conflicting probe modes accepted");
        rejects([&] { parse({"https://example.org/video.mp4", "--probe", "--hud-snapshot", "hud.ppm"}); }, "Probe accepted a HUD snapshot");
        std::cout << "Original CLI option contracts preserved\n";
    });
}
