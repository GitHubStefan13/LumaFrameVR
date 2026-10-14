#include "profile_options.hpp"
#include "presentation_options.hpp"
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
namespace lumaframevr::diagnostics {
namespace {
template <class T>
T number(std::string_view text, std::string_view label) {
    if(text.starts_with('+'))
        text.remove_prefix(1);
    T result{};
    const auto value = std::from_chars(text.data(), text.data() + text.size(), result);
    if(value.ec != std::errc{} || value.ptr != text.data() + text.size())
        throw std::invalid_argument("Invalid " + std::string(label));
    return result;
}
double seconds(std::string_view text, double scale) {
    const auto value = number<double>(text, "duration") * scale;
    if(!std::isfinite(value) || value < 0 || value >= static_cast<double>(std::numeric_limits<std::uint64_t>::max()))
        throw std::invalid_argument("Duration must be finite and nonnegative");
    return std::round(value * 1e9) / 1e9;
}
}  // namespace
std::optional<ProfileOptions> parse_profile_options(std::span<const std::string_view> arguments) {
    ProfileOptions result;
    VideoHints hints;
    bool file{};
    for(std::size_t index = 0; index < arguments.size(); ++index) {
        const auto argument = arguments[index];
        const auto value = [&] {
            if(++index == arguments.size())
                throw std::invalid_argument("Missing value for " + std::string(argument));
            return arguments[index];
        };
        if(argument == "--help" || argument == "-h")
            return std::nullopt;
        if(argument == "--decoder")
            result.decoder = true;
        else if(argument == "--thumbnails")
            result.thumbnails = true;
        else if(argument == "--stats")
            result.stats = true;
        else if(argument == "--projection")
            parse_projection(hints, value());
        else if(argument == "--stereo")
            parse_stereo(hints, value());
        else if(argument == "--sbs-format")
            hints.sbs_format = parse_stereo_format(value());
        else if(argument == "--tb-format")
            hints.tb_format = parse_stereo_format(value());
        else if(argument == "--render") {
            std::array<int, 2> size{};
            for(auto& component : size) {
                component = number<int>(value(), "render dimension");
                if(component <= 0)
                    throw std::invalid_argument("Render dimensions must be positive");
            }
            result.render_size = size;
        } else if(argument == "--repeat") {
            result.repeat = number<unsigned>(value(), "repeat count");
            if(!result.repeat)
                throw std::invalid_argument("Repeat count must be positive");
        } else if(argument == "--seconds")
            result.seconds = seconds(value(), 1);
        else if(argument == "--poll-ms") {
            result.poll_seconds = seconds(value(), 0.001);
            if(result.poll_seconds == 0)
                throw std::invalid_argument("Polling interval must be positive");
        } else if(argument.starts_with('-'))
            throw std::invalid_argument("Unknown option: " + std::string(argument));
        else if(!file) {
            result.file = argument;
            file = true;
        } else {
            const auto target = number<double>(argument, "seek position");
            if(!std::isfinite(target) || target < 0)
                throw std::invalid_argument("Invalid seek position");
            result.targets.push_back(target);
        }
    }
    if(!file)
        throw std::invalid_argument("Usage: profile FILE [SEEK_SECONDS ...] [--help]");
    if(result.thumbnails && (result.decoder || result.render_size))
        throw std::invalid_argument("--thumbnails uses playback and its own preview target");
    if(result.thumbnails && result.targets.empty())
        throw std::invalid_argument("--thumbnails requires seek positions");
    if(result.decoder && result.stats)
        throw std::invalid_argument("--stats measures paced playback; omit --decoder");
    if(result.decoder)
        result.poll_seconds = 0;
    result.presentation = hints.apply(result.presentation);
    return result;
}
}  // namespace lumaframevr::diagnostics
