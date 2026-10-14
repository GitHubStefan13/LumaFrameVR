#include "profile_csv.hpp"
#include <iomanip>
#include <sstream>
#include <vector>
namespace lumaframevr::diagnostics {
namespace {
std::string number(std::optional<double> value, int digits = 6) {
    if(!value)
        return {};
    std::ostringstream out;
    out << std::fixed << std::setprecision(digits) << *value;
    return out.str();
}
std::string join(const std::vector<std::string>& fields) {
    std::string row;
    for(const auto& field : fields) {
        if(!row.empty())
            row += ',';
        if(field.find_first_of(",\r\n\"") != std::string::npos) {
            row += '"';
            for(char c : field) {
                if(c == '"')
                    row += '"';
                row += c;
            }
            row += '"';
        } else
            row += field;
    }
    return row;
}
}  // namespace
std::string ProfileRequest::profile_csv(const ProfileSample& s, std::string_view mode, double poll_ms) const {
    return join({std::to_string(run),
                 std::string(mode),
                 operation,
                 number(target),
                 number(s.elapsed_ms),
                 std::to_string(s.frames),
                 number(s.first),
                 number(s.last),
                 std::to_string(s.skipped),
                 s.eof ? "true" : "false",
                 number(poll_ms, 3),
                 number(s.import_ms),
                 number(s.draw_ms),
                 number(s.gpu_ms),
                 std::to_string(s.eye_size[0]),
                 std::to_string(s.eye_size[1]),
                 number(s.preview_pts),
                 number(s.preview_ms),
                 number(s.preview_ready_ms),
                 number(s.cpu_percent)});
}
std::string ProfileRequest::thumbnail_csv(std::string_view status, const std::optional<ThumbnailProfileSample>& value, double elapsed_ms) const {
    std::vector<std::string> fields{std::to_string(run),           std::to_string(request), operation,           number(target),
                                    repeated ? "repeat" : "first", "uncontrolled",          std::string(status), number(value ? value->sample.elapsed_ms : elapsed_ms)};
    if(value) {
        const auto& s = value->sample;
        for(const auto item : {s.preview_pts, s.preview_ms, s.preview_ready_ms, value->release_handshake_ms, value->worker_wait_ms, value->handoff_ms}) fields.push_back(number(item));
        for(std::size_t i = 0; i < 8; ++i) fields.push_back(value->native ? number(value->native->microseconds[i] / 1000.0) : "");
        for(std::size_t i = 0; i < 8; ++i) fields.push_back(value->native ? std::to_string(value->native->counters[i]) : "");
        for(const auto item : {s.import_ms, s.draw_ms, s.gpu_ms, value->submit_ms, value->fence_wait_ms}) fields.push_back(s.preview_ready_ms ? number(item) : "");
        fields.insert(fields.end(), {std::to_string(s.frames), std::to_string(s.skipped), number(s.first), number(s.audio_clock)});
    } else
        fields.resize(39);
    fields.push_back(scenario);
    return join(fields);
}
void write_profile_row(std::ostream& output, std::string_view row) {
    output << row << '\n';
    output.flush();
    if(!output)
        throw std::runtime_error("Write profiling CSV failed");
}
}  // namespace lumaframevr::diagnostics
