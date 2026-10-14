#include "metrics.hpp"
#include <charconv>
#include <limits>
#include <sstream>
namespace lumaframevr::diagnostics {
std::optional<CpuTimes> cpu_times(std::string_view text) {
    std::istringstream input{std::string(text.substr(0, text.find('\n')))};
    std::string label;
    input >> label;
    if(label != "cpu")
        return std::nullopt;
    std::array<std::uint64_t, 8> values{};
    CpuTimes result{};
    for(auto& value : values) {
        std::string number;
        if(!(input >> number))
            return std::nullopt;
        const auto parsed = std::from_chars(number.data(), number.data() + number.size(), value);
        if(parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size())
            return std::nullopt;
        if(value > std::numeric_limits<std::uint64_t>::max() - result[0])
            return std::nullopt;
        result[0] += value;
    }
    result[1] = values[3] + values[4];
    return result;
}
std::optional<double> cpu_percent(CpuTimes before, CpuTimes after) {
    if(after[0] <= before[0] || after[1] < before[1])
        return std::nullopt;
    const auto total = after[0] - before[0], idle = after[1] - before[1];
    if(idle > total)
        return std::nullopt;
    return (total - idle) * 100.0 / total;
}
std::optional<CpuTimes> memory_bytes(std::string_view text) {
    std::istringstream input{std::string(text)};
    std::string line;
    std::optional<std::uint64_t> total, available;
    while(std::getline(input, line)) {
        std::istringstream fields(line);
        std::string name;
        std::uint64_t value{};
        if(!(fields >> name >> value))
            continue;
        if(name == "MemTotal:")
            total = value;
        if(name == "MemAvailable:")
            available = value;
    }
    if(!total || !available || *available > *total || *total > std::numeric_limits<std::uint64_t>::max() / 1024)
        return std::nullopt;
    return CpuTimes{(*total - *available) * 1024, *total * 1024};
}
}  // namespace lumaframevr::diagnostics
