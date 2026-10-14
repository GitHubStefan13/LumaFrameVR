#include "application_run.hpp"
#include "launch_diagnostics.hpp"
#include "catalogue.hpp"
#include <iostream>
#include <vector>
int main(int argc, char** argv) {
    try {
        lumaframevr::application::install_launch_diagnostics();
        lumaframevr::i18n::active().load(lumaframevr::i18n::default_directory());
        std::vector<std::string_view> arguments;
        for(int index = 1; index < argc; ++index) arguments.emplace_back(argv[index]);
        const auto options = lumaframevr::parse_application_options(arguments);
        if(options.action == lumaframevr::ApplicationAction::help) {
            std::cout << lumaframevr::application_help();
            return 0;
        }
        if(options.action == lumaframevr::ApplicationAction::version) {
            std::cout << "lumaframevr " << LUMAFRAMEVR_VERSION << " (C++ Vulkan/OpenXR)\n"
                      << "Copyright (C) 2026 \n"
                      << "License: GNU GPL version 3 or later <https://www.gnu.org/licenses/gpl-3.0.html>.\n"
                      << "This is free software: you are free to change and redistribute it.\n"
                      << "There is NO WARRANTY, to the extent permitted by law.\n";
            return 0;
        }
        options.apply_decoder_option();
        if(options.probe || options.render_probe) {
            lumaframevr::application::probe(options);
            return 0;
        }
        lumaframevr::application::log_launch_environment(std::cerr);
        return lumaframevr::application::run(options);
    } catch(const std::exception& error) {
        std::cerr << "LumaFrameVR: " << error.what() << '\n';
        return 1;
    }
}
