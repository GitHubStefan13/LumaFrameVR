# AArch64 cross build using the distribution GNU cross toolchain.
#
# This is the simplest cross setup: install the cross compiler and it works,
# with no compiler download and no wrapper scripts.
#
#   sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
#
# The target libraries (Vulkan, OpenXR, FFmpeg, PulseAudio) must be the AArch64
# ones. Point CMAKE_PREFIX_PATH and PKG_CONFIG_LIBDIR at an AArch64 SDK; the host
# x86_64 development packages are not usable for the target.
#
# Use it with:
#   cmake -S . -B build-arm64 -DCMAKE_TOOLCHAIN_FILE=cmake/SteamFrameAarch64Gnu.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Search the host for build tools (glslc, python) but the target for libraries.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Run the tests under QEMU when it is available, so `ctest` works from the host.
find_program(LUMAFRAMEVR_QEMU_PROGRAM qemu-aarch64-static qemu-aarch64)
if(LUMAFRAMEVR_QEMU_PROGRAM)
  set(CMAKE_CROSSCOMPILING_EMULATOR "${LUMAFRAMEVR_QEMU_PROGRAM}" -L /usr/aarch64-linux-gnu)
endif()
