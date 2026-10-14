#pragma once
#include <iosfwd>
namespace lumaframevr::application {
void install_launch_diagnostics();
void log_launch_environment(std::ostream& output);
}  // namespace lumaframevr::application
