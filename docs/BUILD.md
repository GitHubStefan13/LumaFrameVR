# Build and development

The root CMake project builds the C++20 application, GLSL shaders and acceptance tests. 
Python 3.10+ supplies installation tooling and project/installer tests. 
CMake 3.24+ and a C++20 compiler are required.

## Dependencies

Provide a matching SDK with Vulkan headers/loader, a GLSL compiler, OpenXR
headers/loader, JSONCPP, FFmpeg 7 libraries and PulseAudio. The media target checks
libavformat/libavcodec >=61, libavutil >=59 and libswresample >=5 via pkg-config.
None of it travels in the payload: the player links the frame's own FFmpeg, and Vulkan,
OpenXR, PulseAudio and the loaders come from the system.
Do not substitute a network-disabled SDK for a network-enabled Release payload.

### Debian/Ubuntu host packages

On a Debian 13 (trixie) or Ubuntu host the distribution packages satisfy every
requirement, so no separate SDK prefix is needed. This is the exact set verified on
a Debian 13 x86_64 host (GCC 14.2, CMake 3.31):

```sh
sudo apt update && sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  libvulkan-dev vulkan-tools glslc \
  libopenxr-dev \
  libavformat-dev libavcodec-dev libavutil-dev libswresample-dev \
  libpulse-dev libjsoncpp-dev \
  python3-pil
```

`glslc` comes from the `glslc` package (shaderc); `find_package(Vulkan)` locates the
loader from `libvulkan-dev`. The versions that satisfy the media checks are
libavformat/libavcodec 61.x, libavutil 59.x and libswresample 5.x. `python3-pil` is
only needed to redraw the documentation images. With distribution packages in place
the native host build below needs no `SDK` prefix, `PKG_CONFIG_LIBDIR` or
`CMAKE_PREFIX_PATH`:

```sh
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host --parallel 2
```

GCC 14 with C++20 deprecates the free-function `std::atomic_load`/`std::atomic_store`
overloads for `std::shared_ptr`; the tree uses `std::atomic<std::shared_ptr<T>>`
instead so the `-Werror` build stays clean. Older GCC releases accept either form.

Their standalone recipes do not assemble the complete shipping runtime closure;
release packaging requires an already verified payload with exact dependency
notices and corresponding sources.

## Native host

Set SDK to an installed native SDK prefix; adjust pkg-config directories if its
layout differs. Use a fresh build directory when changing compilers or SDKs.

```sh
SDK=/path/to/native-sdk
PKG_CONFIG_LIBDIR="$SDK/lib/pkgconfig" PKG_CONFIG_PATH='' \
BUILD_DIR="$PWD/build-host" ./build.sh \
  -DCMAKE_PREFIX_PATH="$SDK;$SDK/usr" \
  -DLUMAFRAMEVR_ADDITIONAL_TEST_VIDEO=/path/to/video-8s.mp4 \
  -DLUMAFRAMEVR_ASSEMBLY_VIDEO=/path/to/video-8s.mp4
```

The default is Release with two build jobs. Set `BUILD_TYPE=Debug` for local
Debug verification and `JOBS=1` on a memory-constrained host. Player output is
`BUILD_DIR/lumaframevr`; production shaders and branding accompany the build.

For relocated host binaries installed into another prefix, provide the native SDK
library paths as well as its build-time package metadata:

```sh
export LD_LIBRARY_PATH="$SDK/lib:$SDK/usr/lib/x86_64-linux-gnu:$SDK/usr/lib/x86_64-linux-gnu/pulseaudio${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

CMake installation does not vendor dependency libraries. A distributed Release
uses its separately verified runtime closure instead.

## ARM64 cross build

```sh
export LUMAFRAMEVR_SDK_ROOT=/home/talos/matbuild
SDK=/home/talos/matbuild/lumaframevr-arm64-sdk
export PKG_CONFIG_LIBDIR="/home/talos/matbuild/sdk/lib/pkgconfig:$SDK/usr/lib/aarch64-linux-gnu/pkgconfig:$SDK/usr/share/pkgconfig"
cmake -S . -B build-arm64-gnu -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/SteamFrameAarch64Gnu.cmake \
  -DCMAKE_EXE_LINKER_FLAGS="-L$SDK/usr/lib/aarch64-linux-gnu -L/home/talos/matbuild/sdk/lib -L/home/talos/matbuild/lavapipe/arm64root/usr/lib/aarch64-linux-gnu -Wl,--allow-shlib-undefined" \
  -DGLSLC_EXECUTABLE=/home/talos/matbuild/dl/vksdk/bin/glslc \
  -DVulkan_INCLUDE_DIR=/home/talos/matbuild/lumaframevr-host-sdk/usr/include \
  -DVulkan_LIBRARY=/home/talos/matbuild/lavapipe/arm64root/usr/lib/aarch64-linux-gnu/libvulkan.so.1.3.204 \
  -DLUMAFRAMEVR_RUNTIME_DIR=<a directory holding libopenvr_api.so>
cmake --build build-arm64-gnu --target installer --parallel 2
```

`gcc-aarch64-linux-gnu` and `g++-aarch64-linux-gnu` are the compiler, and nothing is wrapped or
downloaded.
Three things in the command above are not obvious. The AArch64 SDK holds no Vulkan headers, so the
header directory and the library are named explicitly. It holds no development symlinks either,
because it is a copy of the frame's runtime libraries, so the linker needs the library directories
on the command line — the toolchain file's environment shadows `LIBRARY_PATH`. The OpenXR loader has
unresolved dependencies of its own that resolve on the frame at runtime, which is why
`--allow-shlib-undefined` is right here rather than a sign of something wrong. And there is no
FFmpeg input at all: the payload ships none, and the player links the frame's own libraries
through the SDK — see [FFmpeg](#ffmpeg-the-frames-own-libraries).

`scripts/make-aarch64-sdk.sh` copies those libraries off the frame over SSH, read-only, so it needs
the frame reachable as `steamos@frame`; it is not self-contained.

Two toolchains are provided. Both need an **AArch64 SDK** for the target libraries
(Vulkan, OpenXR, FFmpeg, PulseAudio); the host x86_64 development packages are not
usable for the target.

| Toolchain file | Compiler | Setup |
| --- | --- | --- |
| `cmake/SteamFrameAarch64Gnu.cmake` | `aarch64-linux-gnu-gcc/g++` | one apt install, no download |

### The AArch64 SDK

The cross compiler provides the compiler and libc, but not the target libraries the
player links against. `scripts/make-aarch64-sdk.sh` assembles them into
`~/lumaframevr-sdk` by copying the libraries and their full dependency closure off the
Steam Frame, read-only. Nothing is installed on the frame.

```sh
./scripts/make-aarch64-sdk.sh          # writes ~/lumaframevr-sdk
```

The script needs SSH access to the frame (`LUMAFRAMEVR_FRAME`, default `steamos@frame`,
and `LUMAFRAMEVR_SSH_KEY`, default `~/.ssh/lumaframe`). It copies the libraries, the
OpenXR loader from the Proton runtime (the frame's `/usr/lib/libopenxr_loader.a` is a
stub with no symbols), and writes the `pkg-config` files. Headers are architecture
independent and come from the host development packages.

The SDK is only needed to **build**. Once built, the installer is self-contained.

### GNU cross toolchain (recommended)

The distribution cross compiler is the simplest path: install it and it works, with no

```sh
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
./scripts/make-aarch64-sdk.sh
cmake --preset frame-aarch64-gnu
cmake --build --preset frame-aarch64-gnu
```

The `frame-aarch64-gnu` preset already points at `~/lumaframevr-sdk` and sets
`PKG_CONFIG_LIBDIR`, so no environment variables are needed. To use a different SDK
location, pass `-DCMAKE_PREFIX_PATH` and `PKG_CONFIG_LIBDIR` yourself:

```sh
SDK=/path/to/arm64-sdk
PKG_CONFIG_LIBDIR="$SDK/pkgconfig" PKG_CONFIG_PATH='' \
cmake -S . -B build-arm64-gnu \
  -DCMAKE_TOOLCHAIN_FILE=cmake/SteamFrameAarch64Gnu.cmake \
  -DCMAKE_PREFIX_PATH="$SDK"
cmake --build build-arm64-gnu --parallel 2
```

QEMU is only needed to run the tests under emulation; the toolchain file picks it up
automatically when `qemu-aarch64-static` is on `PATH`:

```sh
sudo apt install -y qemu-user-static
```

The cross build's `ctest` needs no extra runner script: `cmake/SteamFrameAarch64Gnu.cmake` sets
`CMAKE_CROSSCOMPILING_EMULATOR` to `qemu-aarch64-static` when it is on `PATH`, and ctest uses it for
every test that runs a target binary. Without QEMU the target tests cannot run from the host.

### Building the installer

The `installer` target works from the cross build and produces a genuine AArch64 release
(no `host_architecture` flag), because the player really is AArch64:

```sh
cmake --build --preset frame-aarch64-gnu --target installer
# build-arm64-gnu/dist/lumaframevr-desktop-installer-<version>-1.run
```

The payload carries the player, `launch.sh`, the branding, the compiled shaders and the licences. It
does **not** bundle FFmpeg, Vulkan, OpenXR or PulseAudio: the frame provides those, and the player
links against them. The SDK is only needed to **build**; the installed release
uses the frame's own system libraries.

#### FFmpeg: the frame's own libraries

The payload carries no FFmpeg. The player needs `libavcodec`, `libavformat`, `libavutil` and
`libswresample`, and the Frame already has them: SteamOS ships FFmpeg 7, the AArch64 SDK is a copy
of those same libraries, and the player links them at build time and the loader finds them at
runtime. There is nothing to keep on the build host and nothing to copy into the payload, so
`cmake/AssemblePayload.cmake` has no FFmpeg inputs and `--target package` does not depend on a local
decoder build.

`--decoder hardware` selects FFmpeg's own hwaccel; the default path uses `cpp/media/`'s own V4L2
queue, which is what hands back DRM PRIME frames.

The library audit that travels with the player records the split: every versioned shared object the
ELF lists is `provided_by_system_or_runtime`, and `shipped_in_payload` is empty.

### Selecting the target in VS Code

`CMakePresets.json` defines the configure presets, so the target is chosen from the CMake
Tools status bar instead of by hand:

| Preset | Target |
| --- | --- |
| `native` | x86_64 development host, Release |
| `native-debug` | x86_64 development host, Debug |
| `frame-aarch64-gnu` | Steam Frame, AArch64 cross (GNU) |

In VS Code:

1. Open the **CMake Tools** status bar at the bottom of the window.
2. Click the **preset** name (it shows `native` by default) and pick
   **Steam Frame (AArch64 cross, GNU)**.
3. CMake Tools reconfigures into `build-arm64-gnu/`. Build with the **Build** button or
   `cmake --build --preset frame-aarch64-gnu`.

The same presets work from the terminal:

```sh
cmake --list-presets
cmake --preset frame-aarch64-gnu
cmake --build --preset frame-aarch64-gnu
ctest --preset frame-aarch64-gnu
```

If the status bar still shows only GCC/Clang x86_64, the preset was not picked up: run
**CMake: Delete Cache and Reconfigure** from the command palette, or check that
`CMakePresets.json` is at the workspace root and `cmake.useCMakePresets` is not `never`.

Relocate pkg-config paths to the target SDK, not host libraries. Do not prepend a
sysroot twice to already-absolute SDK paths. Target selection alone is not a
measured ABI guarantee; inspect every distributed ELF and its linked libraries.
The existing Release payload requires GLIBC 2.34 and GLIBCXX 3.4.29.

## Test fixtures

```sh
export LUMAFRAMEVR_TEST_VIDEO=/path/to/video-96x64-2s.mp4
export LUMAFRAMEVR_FORMAT_FIXTURES=/path/to/format-fixtures
export LUMAFRAMEVR_TILE_FIXTURES=/path/to/tile-fixtures
export VK_ICD_FILENAMES=/path/to/working-vulkan-icd.json
ctest --test-dir build-host --output-on-failure
```

Keep the short 96x64 two-second media contract separate from the eight-second
assembly/additional-component fixture. ARM64 tests require a target-compatible ICD,
its complete library closure and guest libraries, and run through the toolchain file's
`CMAKE_CROSSCOMPILING_EMULATOR`, which `cmake/SteamFrameAarch64Gnu.cmake` sets when
`qemu-aarch64-static` is on `PATH`.

The repository ships no fixtures; the CI runner provisions them. A test whose fixture
variable is unset **skips** (exit code 77, reported by CTest as "Skipped"), so a bare
checkout is green rather than red. Four tests skip without fixtures: `timeline` and
`media` need `LUMAFRAMEVR_TEST_VIDEO` (a 96x64, two-second clip), `media_loop` needs
`LUMAFRAMEVR_ASSEMBLY_VIDEO` (a 96x64, **eight-second** clip, set at configure time),
and `image_reader` needs `LUMAFRAMEVR_IMAGE_FIXTURES`. The two media clips are not
interchangeable: `media_loop` seeks to two seconds and drains to EOF.

`scripts/make-test-fixtures.sh` writes both media clips with the `ffmpeg` CLI (from the
`ffmpeg` package), and two more for the codec support test: an AV1 clip and a ten-bit one.
Both are the same pattern as the short clip, which is what lets the ten-bit decode be compared
against the eight-bit one rather than merely reported as decoded. It needs an AV1 encoder
(`libaom-av1`) and a ten-bit one (`libx265`, or `libvpx-vp9`) and says so when it has neither:

```sh
./scripts/make-test-fixtures.sh
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release \
  -DLUMAFRAMEVR_ASSEMBLY_VIDEO="$PWD/test-fixtures/video-8s.mp4" \
  -DLUMAFRAMEVR_CODEC_FIXTURES="$PWD/test-fixtures"
cmake --build build-host --parallel 2
LUMAFRAMEVR_TEST_VIDEO="$PWD/test-fixtures/video-96x64-2s.mp4" ctest --test-dir build-host --output-on-failure
```

`browser` uses `TMPDIR` when set and falls back to `/tmp`, so it needs no setup. The
format, tile and image fixtures have no in-tree generator; supply them or accept that
`image_reader` skips.

The CI runner must provision the SDK and fixtures. Its workflow explicitly checks
required fixture variables and uses CMake/CTest. No remote CI success is implied
by a local run. The default desktop-only installer and bootstrap are documented in [Development](DEVELOPMENT.md).
The retained optional Steam-integrated installer tests can also run independently:

```sh
python3 -B -m unittest discover -s installer/tests -v
python3 -B -m unittest discover -s tests -v
```

## Packaging and safety

See [Installation](INSTALLATION.md) for release operations. `cmake/PackageRelease.cmake` packs an
existing verified ARM64 Release and the current source into the archive; it is not an SDK bootstrapper.
Do not overwrite published versions or use device access for local verification.
