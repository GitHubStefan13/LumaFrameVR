#include "telemetry.hpp"
#include "advanced_layout.hpp"
#include "catalogue.hpp"
#include <iomanip>
#include <sstream>
namespace lumaframevr::diagnostics {
std::vector<ui::PanelRow> draw_information(ui::Canvas& canvas, const std::filesystem::path& path, const Telemetry& telemetry, const DeviceSample& device, double elapsed, double xr_fps,
                                           double target_fps, std::uint64_t skipped, std::string_view playback_status, double position, std::optional<double> duration) {
    canvas.clear();
    const auto columns = static_cast<std::size_t>((canvas.width() - 52) / 12);
    const auto escaped = ui::escape_path(path);
    const auto pages = std::max(std::size_t{1}, (escaped.size() + columns - 1) / columns);
    const auto page = static_cast<std::size_t>(std::max(0.0, elapsed)) / 4 % pages;
    ui::advanced_header(canvas, escaped.substr(page * columns, columns), {});
    auto rows = telemetry.rows(xr_fps, target_fps, skipped);
    const auto percent = [](std::optional<double> value) {
        if(!value)
            return i18n::tr("info.na");
        std::ostringstream out;
        out << std::fixed << std::setprecision(0) << *value << '%';
        return out.str();
    };
    rows.push_back({i18n::tr("info.device"), i18n::tr("info.device_usage", {{"cpu", percent(device.cpu)}, {"gpu", percent(device.gpu)}})});
    std::string memory = i18n::tr("info.na");
    if(device.memory) {
        std::ostringstream used;
        used << std::fixed << std::setprecision(2) << (*device.memory)[0] / 1073741824.0;
        std::ostringstream total;
        total << std::fixed << std::setprecision(2) << (*device.memory)[1] / 1073741824.0;
        memory = i18n::tr("info.memory_usage", {{"used", used.str()}, {"total", total.str()}});
    }
    rows.push_back({i18n::tr("info.memory"), memory});
    rows.push_back({i18n::tr("info.playback"), i18n::tr("telemetry.playback", {{"status", std::string(playback_status)},
                                                                               {"position", ui::timestamp(position)},
                                                                               {"duration", duration ? ui::timestamp(*duration) : i18n::tr("transport.time_unknown")}})});
    std::vector<ui::PanelRow> areas;
    for(std::size_t i = 0; i < rows.size(); ++i) {
        const auto area = ui::information_row(i, canvas.width());
        ui::advanced_line(canvas, area, rows[i].first + "   " + rows[i].second, false, {230, 230, 230, 255});
        areas.push_back({area, i, std::nullopt, false});
    }
    return areas;
}
}  // namespace lumaframevr::diagnostics
