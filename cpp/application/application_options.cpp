#include "application_options.hpp"
#include "presentation_options.hpp"
#include "viewer_preferences.hpp"
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace lumaframevr {
namespace {
double positive_seconds(std::string_view text) {
    double result{};
    if(text.starts_with('+'))
        text.remove_prefix(1);
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if(parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::invalid_argument("Invalid seconds");
    if(!std::isfinite(result) || result <= 0)
        throw std::invalid_argument("Seconds must be positive");
    return result;
}
}  // namespace
StereoFormat parse_stereo_format(std::string_view value) {
    if(value == "full")
        return StereoFormat::full;
    if(value == "half")
        return StereoFormat::half;
    throw std::invalid_argument("Unknown stereo format: " + std::string(value));
}
void parse_projection(VideoHints& hints, std::string_view value) {
    hints.fisheye_degrees = {};
    if(value == "flat")
        hints.projection = Projection::flat;
    else if(value == "180")
        hints.projection = Projection::hemisphere;
    else if(value == "360")
        hints.projection = Projection::sphere;
    else if(value == "fisheye" || value == "fisheye190") {
        hints.projection = Projection::fisheye;
        hints.fisheye_degrees = value == "fisheye190" ? 190U : 180U;
    } else
        throw std::invalid_argument("Unknown projection: " + std::string(value));
}
void parse_stereo(VideoHints& hints, std::string_view value) {
    if(value == "mono")
        hints.stereo = Stereo::mono;
    else if(value == "sbs")
        hints.stereo = Stereo::side_by_side;
    else if(value == "tb")
        hints.stereo = Stereo::top_bottom;
    else
        throw std::invalid_argument("Unknown stereo layout: " + std::string(value));
}
namespace {
void validate(ApplicationOptions& options) {
    if(options.source) {
        options.source = options.source->resolved();
        if(!options.source->remote())
            options.file = options.source->input();
    } else {
        const char* home = std::getenv("HOME");
        const auto directory = home ? std::filesystem::path(home) : std::filesystem::current_path();
        options.file = std::filesystem::is_directory(directory / "Videos") ? directory / "Videos" : directory;
    }
    std::error_code error;
    const bool file = std::filesystem::is_regular_file(options.file, error);
    const bool directory = std::filesystem::is_directory(options.file, error);
    const bool remote = options.source && options.source->remote();
    if(!remote && !file && !directory)
        throw std::invalid_argument("Supply an existing local file or directory, or an http/https/ftp URL; see --help");
    if((options.probe || options.render_probe) && !file && !remote)
        throw std::invalid_argument("Probe modes require a video file or URL");
    if(options.probe && options.render_probe)
        throw std::invalid_argument("Choose one probe mode");
    if(options.hud_snapshot && (options.probe || options.render_probe || (!options.hud && file)))
        throw std::invalid_argument("--hud-snapshot requires the browser or an enabled playback HUD");
    options.presentation = options.overrides.apply(options.presentation);
}
}  // namespace
MediaSource ApplicationOptions::input() const {
    return source ? *source : MediaSource::parse(file.string());
}
void ApplicationOptions::apply_decoder_option() const {
    // Apply once before any decoder/worker starts; no option leaves the environment untouched.
    if(decoder && setenv("LUMAFRAMEVR_DECODER", decoder->c_str(), 1) != 0)
        throw std::runtime_error("Cannot apply --decoder override");
}
ApplicationOptions parse_application_options(std::span<const std::string_view> arguments) {
    ApplicationOptions result;
    for(std::size_t index = 0; index < arguments.size(); ++index) {
        const auto argument = arguments[index];
        const auto value = [&](const char* label) {
            if(++index == arguments.size())
                throw std::invalid_argument(label);
            return arguments[index];
        };
        if(argument == "--version" || argument == "-V") {
            result.action = ApplicationAction::version;
            return result;
        }
        if(argument == "--help" || argument == "-h") {
            result.action = ApplicationAction::help;
            return result;
        }
        if(argument == "--projection")
            parse_projection(result.overrides, value("Missing projection"));
        else if(argument == "--stereo")
            parse_stereo(result.overrides, value("Missing stereo layout"));
        else if(argument == "--sbs-format")
            result.overrides.sbs_format = parse_stereo_format(value("Missing value for --sbs-format"));
        else if(argument == "--tb-format")
            result.overrides.tb_format = parse_stereo_format(value("Missing value for --tb-format"));
        else if(argument == "--decoder") {
            const auto mode = value("Missing value for --decoder");
            if(mode != "auto" && mode != "hardware" && mode != "software" && mode != "v4l2")
                throw std::invalid_argument("Unknown decoder: " + std::string(mode));
            result.decoder = std::string(mode);
        } else if(argument == "--probe")
            result.probe = true;
        else if(argument == "--render-probe")
            result.render_probe = true;
        else if(argument == "--hud")
            result.hud = true;
        else if(argument == "--stats")
            result.stats = true;
        else if(argument == "--seconds")
            result.seconds = positive_seconds(value("Missing seconds"));
        else if(argument == "--snapshot")
            result.snapshot = std::string(value("Missing snapshot path"));
        else if(argument == "--hud-snapshot")
            result.hud_snapshot = std::string(value("Missing snapshot path"));
        else if(argument.starts_with('-'))
            throw std::invalid_argument("Unknown option: " + std::string(argument));
        else if(!result.source)
            result.source = MediaSource::parse(argument);
        else
            throw std::invalid_argument("Only one file or directory is accepted");
    }
    validate(result);
    result.viewer_state = viewer_state_path();
    result.restore_last_folder = !result.source;
    return result;
}
}  // namespace lumaframevr
