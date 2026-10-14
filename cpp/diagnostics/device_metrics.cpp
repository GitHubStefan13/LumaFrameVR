#include "metrics.hpp"
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
namespace lumaframevr::diagnostics {
namespace {
std::string read_text(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}
std::optional<std::filesystem::path> gpu_counter(const std::filesystem::path& dri) {
    std::error_code error;
    for(const auto& entry : std::filesystem::directory_iterator(dri, error)) {
        const auto name = entry.path().filename().string();
        if(name.empty() || name.find_first_not_of("0123456789") != std::string::npos)
            continue;
        const auto counter = entry.path() / "perf_now";
        if(std::filesystem::is_regular_file(counter, error))
            return counter;
    }
    return std::nullopt;
}
}  // namespace
struct DeviceMetrics::State {
    std::filesystem::path proc, dri;
    std::mutex mutex;
    std::condition_variable stop;
    bool stopped{};
    std::optional<DeviceSample> ready;
    std::thread worker;
    void run() {
        const auto gpu = gpu_counter(dri);
        std::optional<CpuTimes> previous;
        for(;;) {
            const auto current = cpu_times(read_text(proc / "stat"));
            DeviceSample sample;
            if(previous && current)
                sample.cpu = cpu_percent(*previous, *current);
            previous = current;
            sample.memory = memory_bytes(read_text(proc / "meminfo"));
            if(gpu) {
                std::istringstream input(read_text(*gpu));
                double value;
                if(input >> value && std::isfinite(value) && value >= 0 && value <= 100)
                    sample.gpu = value;
            }
            std::unique_lock lock(mutex);
            if(!ready)
                ready = sample;
            if(stop.wait_for(lock, std::chrono::milliseconds(500), [&] { return stopped; }))
                return;
        }
    }
};
DeviceMetrics::DeviceMetrics(std::filesystem::path proc, std::filesystem::path dri) : state_(std::make_unique<State>()) {
    state_->proc = std::move(proc);
    state_->dri = std::move(dri);
    state_->worker = std::thread([this] { state_->run(); });
}
DeviceMetrics::~DeviceMetrics() {
    {
        std::lock_guard lock(state_->mutex);
        state_->stopped = true;
    }
    state_->stop.notify_all();
    state_->worker.join();
}
bool DeviceMetrics::refresh() {
    std::lock_guard lock(state_->mutex);
    if(!state_->ready)
        return false;
    latest = *state_->ready;
    state_->ready.reset();
    return true;
}
}  // namespace lumaframevr::diagnostics
