#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")" && pwd -P)
build=${BUILD_DIR:-$root/build}
cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Release}" "$@"
cmake --build "$build" --parallel "${JOBS:-2}"
printf 'Built C++ player: %s/lumaframevr\n' "$build"
