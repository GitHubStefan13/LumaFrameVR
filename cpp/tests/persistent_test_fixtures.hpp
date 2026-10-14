#pragma once
#include <chrono>
#include <filesystem>
#include <unistd.h>
inline std::filesystem::path persistent_test_fixture(const std::filesystem::path& root) {
    return root / ("run-" + std::to_string(getpid()) + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}
