#pragma once
#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
namespace lumaframevr::diagnostics {
using CpuTimes = std::array<std::uint64_t, 2>;
std::optional<CpuTimes> cpu_times(std::string_view text);
std::optional<double> cpu_percent(CpuTimes before, CpuTimes after);
std::optional<CpuTimes> memory_bytes(std::string_view text);
struct DeviceSample {
    std::optional<double> cpu, gpu;
    std::optional<CpuTimes> memory;
};
class DeviceMetrics {
   public:
    explicit DeviceMetrics(std::filesystem::path proc = "/proc", std::filesystem::path dri = "/sys/kernel/debug/dri");
    ~DeviceMetrics();
    DeviceMetrics(const DeviceMetrics&) = delete;
    DeviceMetrics& operator=(const DeviceMetrics&) = delete;
    bool refresh();
    DeviceSample latest;

   private:
    struct State;
    std::unique_ptr<State> state_;
};
}  // namespace lumaframevr::diagnostics
