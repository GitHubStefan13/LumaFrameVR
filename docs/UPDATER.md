# Updating LumaFrameVR, and the update manifest

## Why this is not just "download the new player"

An installation carries more than the player binary: the OpenVR API library, the compiled shaders,
the language files and the licence texts. A release can change any of them, and a library can need
updating on its own. It carries no FFmpeg: the player links the frame's own. The updater therefore
works from a manifest rather than from a version number alone.

## The manifest

A release publishes `latest.json` beside its installer:

```json
{
  "version": "1.0.0",
  "installer": {
    "url": "https://github.com/GitHubStefan13/LumaFrameVR/releases/download/1.0.0/lumaframevr-desktop-installer-1.0.0-2.run",
    "sha256": "689aa2a5c8c65173f241cab133d0d84bce0066e1e4f230ca1e72b596a50c2699",
    "size": 45690667
  },
  "libraries": [
    { "name": "runtime/libopenvr_api.so", "version": "unknown", "sha256": "…" }
  ],
  "notes": "Short summary, shown in the panel."
}
```

`version` is the version `CMakeLists.txt` declares, and the release is tagged with it. `libraries`
lists the real shared-object files in the packaged release tree, with their relative paths and
SHA-256 hashes. Its `version` fields are the version suffixes from those files (ABI/object versions,
not inferred upstream project versions); an unversioned object uses `unknown`. The script compares
these hashes with the installed files and shows changed libraries alongside the release notes.
Both the manifest and installer use HTTPS, including redirects, and published SHA-256 checks.
Packaging also writes `latest.json.sha256`. The checksum and manifest share the release host
trust; these checks detect corruption, not a compromised publisher.

## The button, and who does the work

Settings gains **Check for updates**. The player has no HTTP client by design — it is a Vulkan and
OpenXR program with no network dependencies, and it will not grow one — so the button does not fetch
anything itself. It runs the small script that ships in the installation beside the player:

    update.sh --check        prints AVAILABLE<TAB>version<TAB>changes when newer (exit 0),
                             exits 3 when ours is current or newer, and 1 on failure
    update.sh --install      downloads the installer, verifies it, installs silently, and leaves
                             `current` pointing at the new release

The panel then reads that script's last line and shows it, which gives progress and the reason for a
failure without the player ever opening a socket. When an install completes, the player exits so the
new build is what starts next; the installer keeps the previous release on disk and only moves the
`current` symlink after the payload's checksum verifies, so a failed update leaves the old
installation untouched.

## Panel states

| State | What is shown |
| --- | --- |
| never checked | "Check for updates" |
| checking | the script's current line |
| up to date | "LumaFrameVR <version> is the latest release" |
| available | the new version, and what changes from `libraries` and `notes` |
| failed | the script's last line, verbatim, so the reason is not lost |

## Shipping and verification

The installer target writes `latest.json` and `latest.json.sha256` in `build/dist` after the
installer is complete. Publish both files and the named `.run` as release assets. The installer
URL defaults to the project version tag; `LUMAFRAMEVR_RELEASE_URL` overrides its HTTPS asset
directory, and `LUMAFRAMEVR_RELEASE_NOTES` supplies the notes.

`update.sh` runs the adjacent `update.py`, which needs Python 3.9 or newer with working TLS
certificate validation. It reads the installed version from `BUILD_INFO.json`, refuses a
non-newer release, verifies both the download length and checksum, then invokes the existing
installer with `--update --yes --no-ui --no-steam`. It derives the original per-user prefix from
the installed release path rather than installing a second copy under the default prefix. It
reports success only after reading back `current` and the new release's version. Concurrent
updates of that installation fail instead of sharing a staging directory.

The panel runs the script asynchronously and displays its latest complete line. On an available
release the same measured button offers installation; success requests clean player shutdown.
Long status text wraps by measured glyph widths, including unbroken versions and paths. Details
pages retain the whole message without running into the licence list. The Details button or
left/right on the selected update row advances the page; A starts the check or installation.

The offline updater tests cover string parsing, numeric version ordering, HTTPS redirect
refusal, corrupt manifest and installer checksums, install arguments and the `current` readback.
The native acceptance test uses a local shell fixture to exercise process states, controller
activation, clean exit and measured, paged geometry in every shipped language. These tests do
not open a network connection. A published release still has to exercise the GitHub assets,
redirects and TLS certificates, the AArch64 installer, and the controller interaction on a headset.
