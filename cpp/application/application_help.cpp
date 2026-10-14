#include "application_options.hpp"
namespace lumaframevr {
std::string_view application_help() {
    return "lumaframevr [FILE|DIRECTORY] [--projection flat|180|360|fisheye|fisheye190] [--stereo mono|sbs|tb]\n"
           "[--sbs-format full|half] [--tb-format full|half] [--probe | --render-probe] [--seconds N] [--snapshot FILE.ppm]\n"
           "[--hud] [--hud-snapshot FILE.ppm] [--stats] [--version]\n"
           "[--decoder auto|hardware|software|v4l2] (overrides LUMAFRAMEVR_DECODER)\n"
           "No file: browse ~/Videos. List: up/down selects, left parent/right opens. Grid: arrows move tiles. A opens; B history back.\n"
           "Face buttons: A: Confirm; B: Back (close dialog before leaving playback); X: Rotation; Y: More Settings.\n"
           "Advanced dialogs: open from the bar's gear/turn-arrow buttons; move with the corner dot.\n"
           "Hold either grip: full folder browser (read-only); breadcrumbs, Places, Back/Forward, List/Grid, sort and hidden files.\n"
           "Settings dialog: stick up/down selects; left/right changes.\n"
           "Rotation dialog: sliders; the sticks adjust the video while it is open.\n"
           "Video control: compact icon row beside transport; timeline with times; information, settings, rotation.\n"
           "Images: PNG/JPEG/BMP/WebP/TIFF/GIF/APNG (first frame); ordinary photos default flat/mono.\n"
           "Image bar: previous/next in browser sort order, Back, settings and rotation; no audio or seeking.\n"
           "Image --probe decodes RGBA; --render-probe renders Vulkan without an XR headset.\n"
           "Seeking (video only): d-pad left/right, tap skips 10 s, hold ramps.\n"
           "Audio: speaker button opens vertical volume (up louder/down quieter); separate mute; PulseAudio output.\n"
           "License: GNU GPL version 3 or later; --version prints the notice. No warranty.\n";
}
}  // namespace lumaframevr
