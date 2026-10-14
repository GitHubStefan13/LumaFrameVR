#pragma once
#include "application_options.hpp"
#include <cstddef>

namespace lumaframevr {
struct ProbeResult {
    std::size_t frames{};
    double elapsed{};
};
ProbeResult run_decode_probe(const ApplicationOptions& options);
}  // namespace lumaframevr
