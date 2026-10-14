#include "profile_options.hpp"
namespace lumaframevr::diagnostics {
std::string_view profile_help() {
    return R"(Usage: profile FILE [SEEK_SECONDS ...] [--seconds N] [--repeat N] [--poll-ms N] [--decoder] [--render WIDTH HEIGHT]
Defaults: playback, 1s after each first frame, 1 run, 1ms polling.
--decoder measures synchronous decoding without pacing or polling.
--render adds headless DMA-BUF import and two-eye draw timings through GPU completion.
--thumbnails measures seek stages and 256x144 previews; requires seek positions.
--stats reports shared playback pacing counters to stderr every five seconds.
--projection flat|180|360|fisheye|fisheye190, --stereo mono|sbs|tb, --sbs-format full|half and --tb-format full|half select rendering.
CSV on stdout; diagnostics on stderr. Native calls are guarded by a hard stage watchdog.
)";
}
}  // namespace lumaframevr::diagnostics
