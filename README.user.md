# LumaFrameVR user guide

A native Vulkan/OpenXR video player for Steam Frame, with a read-only folder
browser and stereo playback controls. The application and the native media backend
support H.264, HEVC and VP9; actual hardware decode depends on the target driver.
PQ/HLG content is tone-mapped to SDR on the GPU, not displayed as native HDR.

## Installation

Download the self-extracting installer for the release you want and its checksum companion. In their
folder:

```sh
sha256sum -c lumaframevr-desktop-installer-<version>-<build>.run.sha256
chmod +x lumaframevr-desktop-installer-<version>-<build>.run
./lumaframevr-desktop-installer-<version>-<build>.run --ui
```

Or double-click it. kdialog/zenity dialogs fall back to plain terminal prompts.
Over SSH use `./lumaframevr-desktop-installer-<version>-<build>.run --install --no-steam`.
Normal installation requires no root and no Python. Python 3 is needed only for
optional Steam non-Steam-game shortcut editing.

In a directory bundle, use `./install.sh`. Actions: `--install` (default),
`--repair`, `--uninstall [--purge]`, `--steam-add`, `--steam-remove`, `--plan`.
Options: `--ui` / `--no-ui`, `--steam` / `--no-steam` (ask by default), `--yes`,
`--prefix DIR`, `--payload FILE`, `--help`. The default prefix is `~/.local`:
`opt/lumaframevr/releases/<version>/`, `opt/lumaframevr/current`, `bin/lumaframevr`,
desktop entry, icon, install record and state log. Settings in
`~/.config/lumaframevr` and cache in `~/.cache/lumaframevr` survive uninstall
unless `--purge` is used. Read [Installation](docs/INSTALLATION.md) for details.

## Playback

```sh
~/.local/bin/lumaframevr
~/.local/bin/lumaframevr /path/to/videos
~/.local/bin/lumaframevr /path/to/video.mp4 --projection 360 --stereo tb
~/.local/bin/lumaframevr --help
```

Without a path, browsing starts in `~/Videos`, or the home directory if it does
not exist. A directory starts the browser there; a file starts playback.

- Projection: `flat`, `180`, `360`, `fisheye` or `fisheye190`.
- Stereo: `mono`, `sbs` or `tb`; left/top is the normal left-eye image.
- `--sbs-format full|half` and `--tb-format full|half` select per-eye proportions.
- Explicit options override supported format hints. Per-video settings are remembered
  until exit. The folder browser and the transport remember the last folder, volume and
  mute state across launches unless remembering is turned off in Viewer advanced settings.

## Browser and controls

The browser has places, breadcrumbs, back/forward/up/refresh, list and grid views, A–Z and
Z–A sorting, and a Settings entry. Settings holds the hidden-file control, optional persistent
folder thumbnails, the remembered-state control and the interface language. Browsing never
modifies files, and never mounts or ejects devices; documents stay visible, but only
playable videos open in the player.

| Input | What it does |
| --- | --- |
| A | Confirms a pointed control, or opens the selected browser entry. |
| B | Goes back, and closes a dialog before it leaves playback. |
| X | Toggles Rotation. |
| Y | Toggles More Settings. |
| Sticks | In the list, up/down selects, left visits the parent and right opens. In the grid, left/right moves one tile and up/down moves a row. |
| Either inner grip | Holds the folder browser open. |
| Trigger | Activates controls. Dragging the browser scrolls, and releasing after a scroll does not open a file by accident. |

The playback bar carries previous/play-pause/next, a timeline, the speaker, information,
minimise, settings, rotation and grab controls. The speaker opens a vertical slider (up
increases, down decreases); mute is separate, and B, another control or minimise closes the
popup. Dialogs stay where they are placed: hold the top-right grab dot with the trigger to
move one, and release to leave it there. Rotation offers a mode selector, draggable alignment
sliders and a reset, and while it is open the sticks adjust orientation and zoom, or stereo
position, according to the selected mode. Sticks select settings rows and change values; up
selects the preceding row. D-pad left/right taps skip ten seconds and holding accelerates the
repeats; the sticks do not seek during ordinary playback.

For flat stereo movies choose Flat, then Side by side or Top/bottom. Full keeps
each half's proportions; Half expands horizontally for SBS or vertically for
Top/bottom. Video settings do not contain the transport's volume controls.

## Controllers and surroundings

Controller bodies use actual SteamVR runtime models. Missing runtime models or
grip alignment are reported; test-fixture meshes are not used as substitutes.
The eye buttons share one state between file viewer and video controls and start
off. They enable surroundings only when the runtime advertises a usable
passthrough backend. Unsupported capability leaves the control disabled.
See [Controller models](docs/CONTROLLERS.md) and [Passthrough](docs/PASSTHROUGH.md).

## Diagnostics and limits

`--stats`, `--hud`, `--snapshot` and `--hud-snapshot` are optional diagnostics;
`--probe` checks decoding without establishing headset rendering.
Startup log: `${XDG_STATE_HOME:-$HOME/.local/state}/lumaframevr/startup.log`.
Physical compositor, VPU, Main10/HDR appearance, audio, camera and sustained
performance checks remain device-specific and are not covered by local tests.

LumaFrameVR is free software under the GNU GPL version 3 or later; see [LICENSE](LICENSE)
Dependency notices and corresponding sources travel with the Release package. Published packages
retain their own evidence; this guide does not assert a new license or public
issue-reporting endpoint.
