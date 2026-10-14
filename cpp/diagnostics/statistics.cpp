#include "statistics.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
namespace lumaframevr::diagnostics {
void Statistics::count(std::string name, std::uint64_t value) {
    counts_[std::move(name)] += value;
}
void Statistics::sample(std::string name, double value) {
    auto& sample = samples_[std::move(name)];
    if(!sample.count)
        sample.min = sample.max = value;
    ++sample.count;
    sample.total += value;
    sample.min = std::min(sample.min, value);
    sample.max = std::max(sample.max, value);
}
void Statistics::merge(const Statistics& other) {
    for(const auto& [name, count] : other.counts_) this->count(name, count);
    for(const auto& [name, source] : other.samples_) {
        auto& target = samples_[name];
        if(!target.count)
            target = source;
        else {
            target.count += source.count;
            target.total += source.total;
            target.min = std::min(target.min, source.min);
            target.max = std::max(target.max, source.max);
        }
    }
}
std::string Statistics::report(std::string_view scope, double seconds) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << "Stats: scope=" << scope << " seconds=" << seconds;
    for(const auto& [name, count] : counts_) out << ' ' << name << '=' << count;
    for(const auto& [name, sample] : samples_)
        out << ' ' << name << "_n=" << sample.count << ' ' << name << "_mean=" << sample.total / sample.count << ' ' << name << "_min=" << sample.min << ' ' << name << "_max=" << sample.max;
    counts_.clear();
    samples_.clear();
    return out.str();
}
Performance::Performance(bool diagnostic, double now) : period_(now), report_(now) {
    if(diagnostic)
        stats.emplace();
}
void Performance::record(bool uploaded, std::int64_t period_ns, double work_ms, bool hud, double now) {
    ++frames_;
    videos_ += uploaded;
    target_fps = 1e9 / std::max(std::int64_t{1}, period_ns);
    if(stats) {
        if(previous_)
            stats->sample("interval_ms", (now - *previous_) * 1000);
        stats->sample("work_ms", work_ms);
        stats->count("submitted", 1);
        stats->count("fresh_video", uploaded);
        stats->count("hud", hud);
    }
    previous_ = now;
    const auto elapsed = now - period_;
    if(elapsed >= 0.5) {
        xr_fps = frames_ / elapsed;
        video_fps = videos_ / elapsed;
        frames_ = videos_ = 0;
        period_ = now;
    }
}
void Performance::suspend(double now) {
    previous_.reset();
    frames_ = videos_ = 0;
    period_ = now;
}
std::string Performance::report(double now, bool final_report, Statistics& renderer) {
    if(!stats || (!final_report && now - report_ < 5))
        return {};
    stats->merge(renderer);
    renderer = {};
    const auto result = stats->report("xr", now - report_);
    *stats = {};
    report_ = now;
    return result;
}
}  // namespace lumaframevr::diagnostics
