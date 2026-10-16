# Developer setup

## Tools and checkout

You need Linux, Git, CMake 3.24 or newer, a C++20 compiler, Python 3.11+ and Bash. The verified
builds use two jobs; on a memory-limited machine do not quietly raise that number. Clone the
repository, enter its root, and pick one of the two build paths below. No build or test step
needs a Steam Frame.

On Debian 13 (trixie) or Ubuntu the distribution packages cover everything, so the full
player builds without a separate SDK prefix. This is the set verified on a Debian 13 x86_64
host (GCC 14.2, CMake 3.31, 12 cores):

```sh
sudo apt update && sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  libvulkan-dev vulkan-tools glslc \
  libopenxr-dev \
  libavformat-dev libavcodec-dev libavutil-dev libswresample-dev \
  libpulse-dev libjsoncpp-dev \
  python3-pil
```

`glslc` is the GLSL compiler, `libvulkan-dev` supplies the loader, and the FFmpeg 7 dev
packages (libavformat/libavcodec 61.x, libavutil 59.x, libswresample 5.x) satisfy the media
checks. `python3-pil` is only for the documentation images. With these installed the full
native build needs no `SDK`, `PKG_CONFIG_LIBDIR` or `CMAKE_PREFIX_PATH`:

```sh
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host --parallel 2
```

GCC 14 with C++20 deprecates the free-function `std::atomic_load`/`std::atomic_store`
overloads for `std::shared_ptr`; the tree uses `std::atomic<std::shared_ptr<T>>` so the
`-Werror` build stays clean. Older GCC releases accept either form.

### VS Code CMake Tools

The checked-in `.vscode/settings.json` pins `"cmake.generator": "Ninja"` and
`"cmake.buildDirectory": "${workspaceFolder}/build"`. This overrides any user-level
`cmake.generator` (for example a Windows `NMake Makefiles` setting inherited over a
remote connection), which does not exist on Linux and makes CMake Tools fail with
"Could not create named generator NMake Makefiles". Ninja is installed by the package
list above; `Unix Makefiles` works too if you prefer it.

## Fast component-only build

This parses the sources and checks browsing, input math, presentation and the UI contracts
without Vulkan, OpenXR or the media SDKs. It deliberately does not produce the full player.

```sh
cmake -S . -B build-components -DCMAKE_BUILD_TYPE=Release -DLUMAFRAMEVR_BUILD_VULKAN=OFF -DLUMAFRAMEVR_BUILD_MEDIA=OFF
cmake --build build-components --parallel 2
TMPDIR=/tmp ctest --test-dir build-components --output-on-failure
python3 -B -m unittest discover -s tests -v
python3 -B -m unittest discover -s installer/tests -p 'test_install_sh.py' -v
```

`browser_acceptance` reads `TMPDIR` for its scratch directory and fails with "scratch
directory required" when it is unset, so export `TMPDIR=/tmp` (or any writable directory)
before running CTest. The component build needs no Vulkan, OpenXR or media SDK and passes
all 21 tests on a bare Debian host.

## Full native player

With the Debian/Ubuntu packages above installed, the full player builds directly; the
`SDK` prefix, `PKG_CONFIG_LIBDIR` and `CMAKE_PREFIX_PATH` below are only needed when you
build against a separate SDK tree instead of distribution packages. The CMake media checks
want libavformat and libavcodec >= 61, libavutil >= 59 and libswresample >= 5, which the
Debian 13 packages provide.

```sh
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release \
  -DLUMAFRAMEVR_ADDITIONAL_TEST_VIDEO=/absolute/path/to/video-8s.mp4 \
  -DLUMAFRAMEVR_ASSEMBLY_VIDEO=/absolute/path/to/video-8s.mp4
cmake --build build-host --parallel 2
```

Against a separate SDK tree, point the build at it instead:

```sh
SDK=/absolute/path/to/native-sdk
export PKG_CONFIG_LIBDIR="$SDK/lib/pkgconfig"
export PKG_CONFIG_PATH=''
export LD_LIBRARY_PATH="$SDK/lib:$SDK/usr/lib/x86_64-linux-gnu:$SDK/usr/lib/x86_64-linux-gnu/pulseaudio"
BUILD_DIR="$PWD/build-host" ./build.sh -DCMAKE_PREFIX_PATH="$SDK;$SDK/usr" -DGLSLC_EXECUTABLE=/absolute/path/to/glslc -DLUMAFRAMEVR_ADDITIONAL_TEST_VIDEO=/absolute/path/to/video-8s.mp4 -DLUMAFRAMEVR_ASSEMBLY_VIDEO=/absolute/path/to/video-8s.mp4
```

[Build](BUILD.md) covers the fixture variables, the ABI constraints and SDK relocation.
`cmake --install` vendors nothing, so a shipping package needs a verified runtime closure and
the notices and corresponding sources of what it actually contains.

## Test fixtures

The repository ships no fixtures; the CI runner provisions them. A test whose optional
fixture is absent **skips** (exit code 77, reported by CTest as "Skipped"), so a bare
checkout is green rather than red. A fixture that is present but wrong still fails, which is
the point: the test only stays quiet when it has nothing to check.

Four tests skip on a bare checkout, each for a missing fixture:

| Test | Skips when | Provide with |
| --- | --- | --- |
| `timeline` | `LUMAFRAMEVR_TEST_VIDEO` is unset | a 96x64, **two-second** clip |
| `media` | `LUMAFRAMEVR_TEST_VIDEO` is unset | the same two-second clip |
| `media_loop` | `LUMAFRAMEVR_ASSEMBLY_VIDEO` is unset | a 96x64, **eight-second** clip |
| `image_reader` | `LUMAFRAMEVR_IMAGE_FIXTURES` is unset | a directory of `corners.*` images |

The two media clips are **not interchangeable**. `timeline` and `media` use the short
two-second contract; `media_loop` seeks to two seconds and drains to EOF, so it needs the
eight-second clip. Pointing `LUMAFRAMEVR_ASSEMBLY_VIDEO` at the two-second clip makes
`media_loop` fail with "real media deadline exceeded".

### Generating the media fixtures

`scripts/make-test-fixtures.sh` writes both clips with the `ffmpeg` CLI (install it first,
for example `sudo apt install ffmpeg`):

```sh
./scripts/make-test-fixtures.sh          # writes ./test-fixtures/
```

Then configure and run with both fixtures:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DLUMAFRAMEVR_ASSEMBLY_VIDEO="$PWD/test-fixtures/video-8s.mp4"
cmake --build build --parallel 2
LUMAFRAMEVR_TEST_VIDEO="$PWD/test-fixtures/video-96x64-2s.mp4" ctest --test-dir build --output-on-failure
```

`LUMAFRAMEVR_ASSEMBLY_VIDEO` is a CMake cache variable, so it is set at configure time;
`LUMAFRAMEVR_TEST_VIDEO` is read from the environment at test time. Without them the four
tests above skip and the rest still run.

`LUMAFRAMEVR_FORMAT_FIXTURES` (full-range/Main10/HDR10 clips), `LUMAFRAMEVR_TILE_FIXTURES`
(red/green/blue/grey clips) and `LUMAFRAMEVR_IMAGE_FIXTURES` (96x64 `corners.*`, EXIF, alpha
and palette images) have no in-tree generator; supply them or accept that `image_reader`
skips. `browser` uses `TMPDIR` when set and falls back to `/tmp`, so it runs without any
checkout-only checks skip.

## Test policy

The suite is deliberately small and covers main functionality only. It is not a
regression net for every internal detail: a test that pins an implementation choice
makes the code harder to change than the feature is worth. The rule is to assert
behaviour, not the literal values a developer happened to pick.

The CTest suite is the core set. Every acceptance test builds into one binary,
`lumaframevr_tests`, and is selected by name (`lumaframevr_tests <test> [args]`); run it
with no arguments to list the tests. One binary keeps the build graph small while each test
still runs as its own CTest case with its own arguments and timeout.

| Test | Covers |
| --- | --- |
| `catalogue` | language catalogue loading |
| `browser` | folder listing and navigation |
| `ui_contract` | UI layout contract |
| `controller` | pose filter, pointer and seek math |
| `presentation` | projection and alignment math |
| `gpu_color` | GPU colour conversion |
| `timeline` | playback timeline (needs `LUMAFRAMEVR_TEST_VIDEO`) |
| `media` | decode, seek and drain (needs `LUMAFRAMEVR_TEST_VIDEO`) |
| `image_reader` | still-image decode (needs `LUMAFRAMEVR_IMAGE_FIXTURES`) |
| `image_loader` | image loader |
| `source` | media source parsing |
| `application_options` | CLI option parsing |
| `controller_bindings` | controller action schema and bindings |
| `localization` | localized strings |
| `routing` | file-type routing |
| `media_loop` | end-to-end media loop (needs `LUMAFRAMEVR_ASSEMBLY_VIDEO`) |
| `browser_viewport` | browser viewport |
| `launch_diagnostics_acceptance` | launch diagnostics |
| `installer_acceptance` | POSIX installer end to end |
| `launcher_acceptance` | launcher environment |
| `application_cli_help`, `application_cli_version`, `application_cli_probe` | CLI surface |
| `shader_module` | shader module loading |

`controller_bindings` is the model for the policy. It checks that every controller profile
binds the actions the application needs and that each binding's path suffix matches the
action's declared type. It does **not** assert which physical button carries an action, so
remapping a face button (for example moving confirm from A to B) is a one-line change in
`cpp/headset/controller_bindings.cpp` with no test edit. The exact button is a product
decision, not a contract.

The installer, bootstrap and packaging suites under `installer/tests`,
`github_bootstrap` and `github_packaging/tests` are separate and run on their own; they
need no fixtures and no headset.

## ARM64 cross compilation

Two toolchains are provided; both need an AArch64 SDK for the target libraries. The GNU
cross compiler is the simplest, because it is one apt install with no download:

```sh
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
cmake --preset frame-aarch64-gnu
cmake --build --preset frame-aarch64-gnu
```

That is the whole cross setup. The toolchain file sets `CMAKE_CROSSCOMPILING_EMULATOR` when
`qemu-aarch64-static` is on `PATH`, so `ctest` runs the target tests from the host with no runner
script and no environment variables of its own. Tests that run under QEMU say nothing about the
headset runtime. [Build](BUILD.md#arm64-cross-build)
covers the presets and how to pick the target in VS Code.

## Shell installer and packaging tests

The end-to-end suite builds miniature ARM64-marked payload fixtures and runs the real POSIX
installer in throwaway HOME directories. It needs no release archive, no SteamVR and no
headset, and its fixture executables are not VR acceptance evidence.

### Building an installer

Packaging is driven by CMake, so the version is never hardcoded: it comes from the
`project(... VERSION ...)` line in the top level `CMakeLists.txt`. Bumping the release is a
one line change, and every artifact name follows automatically.

Packaging produces a release, so the `installer` target only exists in a Release build. In a
Debug build it is not offered at all, and CMake says so at configure time. `package` is an
alias for the same chain, so either name works.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target installer    # or: --target package
```

One target runs the whole chain and writes everything under `build/dist`:

1. assemble the flat payload from the built player, `launch.sh`,
   `branding/` and the compiled shaders, plus `BUILD_INFO.json` and `SHA256SUMS.txt`;
2. pack the release archive `lumaframevr-steam-frame-arm64-<version>.tar.gz` and its `.sha256`;
3. build the self-extracting installer `lumaframevr-desktop-installer-<version>-<build>.run`
   and the directory bundle.

The target clears `build/dist` first, so it is repeatable: run it as often as you like. That
is safe because `build/dist` is a build directory, not a published location; the packaging step
still refuses to overwrite a published release outside the build tree.

The payload carries no FFmpeg: the player links the frame's own decoder libraries at build time
from the AArch64 SDK and the loader finds them on the frame at runtime, so the assembly step has
nothing to copy and `--target package` needs no local decoder build. The audit the target writes
next to the player records that split. [FFmpeg: the frame's own libraries](BUILD.md#ffmpeg-the-frames-own-libraries)
has the rule.

### Running the installer where the `.run` cannot execute

Some targets, the Steam Frame included, cannot execute the self-extracting `.run` directly:
the file may arrive without its execute bit, or the filesystem may be mounted `noexec`. The
directory bundle ships `install-lumaframevr.desktop` beside `install.sh`, and the frame
executes that entry directly. Failing that, `sh <run>` runs the same installer without the
file needing an execute bit of its own, and every installer option passes through unchanged.

The installer builder is the script in `scripts/`; the version and the paths come from CMake.
`LUMAFRAMEVR_INSTALLER_BUILD` sets the installer build number (default `1`).

### Target architecture

A release targets the Steam Frame, so the packaging scripts enforce an **AArch64** player by
default. Building on an x86_64 development host produces an x86_64 player, which the scripts
refuse with "The player is not a Linux AArch64 ELF64 binary". To package such a build for
local testing only, opt in explicitly:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLUMAFRAMEVR_ALLOW_HOST_ARCHITECTURE=ON
```

The resulting archive is marked `host_architecture` and is never a release. A cross build for
the Frame needs no flag.

### File associations

The installer registers LumaFrameVR as the default handler for the media types the player
opens (the list in `cpp/source/media_file_type.hpp`), so clicking a video or image in the
file manager opens the player. The desktop entry carries the matching `MimeType=` line and
the associations are written to `~/.config/mimeapps.list` under `[Default Applications]`.

The associations are written directly rather than through `xdg-mime`, because `xdg-mime`
silently drops its very first write when it has to create `mimeapps.list`, which loses one
type. Uninstall removes only the entries that point at this installation, leaving any other
handler the user chose in place.

### Steam shortcut

The installer adds LumaFrameVR to Steam as a non-Steam game when asked (`--steam`, or the
prompt). It writes `shortcuts.vdf` directly; Steam picks the change up without a restart. The
Steam step is not fatal to the install: if it fails, the install still completes and the
message says how to add the shortcut later.

### Library paths

The player links against the frame's own libraries (Vulkan, FFmpeg, PulseAudio), so the
payload does not bundle them. Some of those libraries, the OpenXR loader in particular, are
not in the default loader path on the frame. At install time the installer runs `ldd` on the
player, finds the directories that hold any library the loader cannot resolve, and bakes
those directories into the launcher's `LD_LIBRARY_PATH`. It searches the standard library
directories, `/opt/steamvr`, and the Steam library folders (SteamVR, Proton). A library that
cannot be found is reported as a warning, so a missing dependency is visible at install time
rather than as a start-up failure. `launch.sh` adds the same well-known directories as a
fallback.

### SteamVR environment

The launcher sets `XR_RUNTIME_JSON` to the frame's SteamVR runtime when that manifest exists.
It sets `VR_PATHREG_OVERRIDE` and `VR_LOG_PATH` only when the target already exists: an
override that names a missing file makes the OpenXR loader skip the runtime with
"negotiation failed", so a missing registry is left to SteamVR's own defaults.

Steam sets `ENABLE_GAMESCOPE_WSI=1` when it launches a game on a gamescope session (the Steam
Frame and Steam Deck). The launcher only sets `DISABLE_GAMESCOPE_WSI=1` when Steam has not
asked for gamescope, so the two never contradict each other.

The Steam AppID is deterministic (a CRC of the launcher path and the name), so the installer
computes it and bakes it into the launcher. The launcher exports `SteamAppId` and
`SteamGameId` when they are not already set, so the OpenVR overlay and the Steam runtime see
the same AppID whether the player is started from Steam, the desktop entry or a terminal.
Steam's own values always win when Steam launches the game.

`build-installer.py` refuses a player that is not an AArch64 ELF, because a
real release targets the Frame. A native development build is not AArch64, so the target
passes `--allow-host-architecture` there automatically; a cross build still enforces AArch64.
An archive built on the host is marked `host_architecture` and is for local testing only,
never a release.

The installer builder can still be run directly when you want to control every argument:

```sh
python3 -B scripts/build-installer.py --version <version> \
  --payload "$PWD/dist/lumaframevr-steam-frame-arm64-<version>.tar.gz" \
  --output "$PWD/dist/lumaframevr-desktop-installer-<version>-<build>.run" \
  --directory "$PWD/dist/lumaframevr-installer-<version>"
(cd dist && sha256sum -c lumaframevr-desktop-installer-<version>-<build>.run.sha256)
```

`build-installer.py` validates the payload before writing anything: it must carry the
release name, `launch.sh`, the application binary and a `SHA256SUMS.txt`, and the binary
must be an AArch64 executable. The `.run` is the installer script, the payload and the
Steam helper in one file; the directory bundle is the same parts as separate files plus a
double-clickable desktop entry. Both are written with a checksum companion.

installed, and redraws `docs/images/` from the actual top level listing.

```sh
timeout 300 python3 -B -m unittest discover -s installer/tests -p 'test_install_sh.py' -v
sh -n installer/install.sh.in
python3 -B scripts/verify_application.py --help
```

The builder writes the checksum companion next to the executable. Python is developer tooling
here, not something a normal installation needs: in the directory bundle run
`./install.sh --ui`, or `./install.sh --install --no-steam --no-ui --yes` over SSH.
[Installation](INSTALLATION.md) lists the actions, the options, the per-user `~/.local` layout
and the optional Steam shortcut. Only that Steam step needs Python 3, and the prompts fall
back to the terminal when kdialog and zenity are both missing.

Changing source does not change an executable you already built. Never overwrite a published
artifact: changed bytes get a new version.

The bootstrap tests keep their evidence under `artifacts/`, and they install, reinstall,
repair and uninstall without touching your own HOME. `GITHUB_BOOTSTRAP_ASSET` and
`GITHUB_BOOTSTRAP_PAYLOAD` point them at real artifacts; without those they build labelled
fixtures. [Bootstrap details](../github_bootstrap/README.md) has the rest.

## Environment variables

Runtime, read by the launcher or the installer:

| Variable | Effect |
| --- | --- |
| `XR_RUNTIME_JSON` | OpenXR runtime to use. The launcher defaults to `/opt/steamvr/steamxr_linuxarm64.json` when that file exists. |
| `VR_PATHREG_OVERRIDE`, `VR_LOG_PATH` | Where SteamVR keeps its registry and logs. The launcher points these at the user's config and state directories. |
| `LUMAFRAMEVR_PACKAGE_ROOT`, `LUMAFRAMEVR_LOG_FILE` | Set by `launch.sh`. |
| `LUMAFRAMEVR_VIEWER_STATE` | Path of the viewer state file. An empty value disables persistence, which the tests use. |
| `LUMAFRAMEVR_DECODER` | Force a decoder. `software` skips the VPU, for diagnosis. |
| `LUMAFRAMEVR_DECODE_STALL_MS` | How long to wait for the first hardware frame before falling back to software. Diagnosis only; the shipped default is 4000. |
| `LUMAFRAMEVR_PREFIX` | Install prefix, the same as `--prefix`. |
| `LUMAFRAMEVR_STEAM_USERDATA` | Steam userdata directory, for a Steam installation in a non-standard place. |
| `LUMAFRAMEVR_INSTALLER_FREE_KB` | Overrides the free-space check. Tests use it; leave it unset. |

Build and test:

| Variable | Effect |
| --- | --- |

| `LUMAFRAMEVR_RUNTIME_LIBS`, `LUMAFRAMEVR_EXTRA_LIBS` | Extra guest libraries for the cross build. |
| `LUMAFRAMEVR_TEST_VIDEO`, `LUMAFRAMEVR_AUDIO_VIDEO`, `LUMAFRAMEVR_FORMAT_FIXTURES`, `LUMAFRAMEVR_TILE_FIXTURES`, `LUMAFRAMEVR_IMAGE_FIXTURES`, `LUMAFRAMEVR_SHADER_DIR` | Fixtures the tests read. |
| `GITHUB_BOOTSTRAP_ASSET`, `GITHUB_BOOTSTRAP_PAYLOAD`, `GITHUB_BOOTSTRAP_EVIDENCE` | Artifacts and evidence directory for the bootstrap suite. |

## GitHub CI

Ordinary pull-request and push CI builds the dependency-light Release components and runs the
workflow wants a trusted self-hosted developer runner with the SDK, an ICD and the fixtures,
and it must not run untrusted pull-request code by itself. Release assets are prepared locally
from an already verified release; CI neither invents a missing SDK nor stands in for hardware
proof, and a prepared workflow is not a run.

## Contribution and acceptance

Read [Contributing](../CONTRIBUTING.md), the [C++ guidelines](CPP-GUIDELINES.md) and the
[AI usage policy](../AI-USAGE.md), then [Architecture](ARCHITECTURE.md). Keep the C++20,
GLSL behavior intact, write the test before you change behavior, compile every
subsystem you touched, and keep the required notices where they are. A documentation change
does not imply a new dependency or an unrelated refactor.

Useful entry points: `cpp/application/application_run.cpp`, the component CMake files,
`installer/install.sh.in`, `installer/steam_shortcut.py` and `scripts/build-installer.py`. The
shutdown defect is tracked in [Known issues](KNOWN-ISSUES.md) rather than hidden behind tests
that pass.
