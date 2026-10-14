#include "application_options.hpp"
#include "decode_probe.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argc, char** argv) {
    try {
        std::vector<std::string_view> arguments;
        for(int index = 1; index < argc; ++index) arguments.emplace_back(argv[index]);
        const auto options = lumaframevr::parse_application_options(arguments);
        if(options.action == lumaframevr::ApplicationAction::version) {
            std::cout << "lumaframevr-probe " << LUMAFRAMEVR_VERSION << '\n';
            return 0;
        }
        if(options.action == lumaframevr::ApplicationAction::help) {
            std::cout << "Standalone C++ diagnostic runner; use --probe or --render-probe.\n" << lumaframevr::application_help();
            return 0;
        }
        if(!options.probe && !options.render_probe)
            throw std::invalid_argument("Use --probe or --render-probe with this diagnostic runner");
        options.apply_decoder_option();
        lumaframevr::run_decode_probe(options);
        return 0;
    } catch(const std::exception& error) {
        std::cerr << "LumaFrameVR probe: " << error.what() << '\n';
        return 1;
    }
}
