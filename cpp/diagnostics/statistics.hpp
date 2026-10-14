#pragma once
#include <chrono>
#include <map>
#include <optional>
#include <string>
namespace lumaframevr::diagnostics {
struct Samples {
    std::uint64_t count{};
    double total{}, min{}, max{};
};
class Statistics {
   public:
    void count(std::string name, std::uint64_t value);
    void sample(std::string name, double value);
    void merge(const Statistics& other);
    std::string report(std::string_view scope, double seconds);

   private:
    std::map<std::string, std::uint64_t> counts_;
    std::map<std::string, Samples> samples_;
};
class Performance {
   public:
    explicit Performance(bool diagnostic, double now);
    void record(bool uploaded, std::int64_t period_ns, double work_ms, bool hud, double now);
    void suspend(double now);
    std::string report(double now, bool final_report, Statistics& renderer);
    double xr_fps{}, video_fps{}, target_fps{};
    std::optional<Statistics> stats;

   private:
    double period_, report_;
    std::optional<double> previous_;
    std::uint64_t frames_{}, videos_{};
};
}  // namespace lumaframevr::diagnostics
