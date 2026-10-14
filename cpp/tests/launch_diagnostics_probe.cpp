#include "launch_diagnostics.hpp"
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sys/resource.h>
int main(int argc, char** argv) {
    if(argc != 2)
        return 2;
    rlimit no_core{};
    setrlimit(RLIMIT_CORE, &no_core);
    lumaframevr::application::install_launch_diagnostics();
    lumaframevr::application::log_launch_environment(std::cerr);
    std::raise(std::atoi(argv[1]));
    return 3;
}
