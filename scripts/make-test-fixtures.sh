#!/usr/bin/env bash
# Generate the media fixtures the acceptance tests read.
#
# The tests skip when a fixture is absent, so this is optional: run it when you
# want the media tests to actually execute. It needs the ffmpeg CLI.
#
# Two clips, because the tests use two different contracts:
#   video-96x64-2s.mp4   the short media contract (LUMAFRAMEVR_TEST_VIDEO)
#   video-8s.mp4         the eight-second assembly fixture (LUMAFRAMEVR_ASSEMBLY_VIDEO)
#
# Usage: scripts/make-test-fixtures.sh [output-directory]
set -euo pipefail

directory=${1:-$PWD/test-fixtures}
mkdir -p -- "$directory"

if ! command -v ffmpeg >/dev/null 2>&1; then
    printf 'ffmpeg is required; install it first (for example: sudo apt install ffmpeg)\n' >&2
    exit 1
fi

short=$directory/video-96x64-2s.mp4
long=$directory/video-8s.mp4

printf 'Writing %s\n' "$short"
ffmpeg -y -loglevel error -f lavfi -i testsrc=size=96x64:rate=30 -t 2 -pix_fmt yuv420p "$short"

printf 'Writing %s\n' "$long"
ffmpeg -y -loglevel error -f lavfi -i testsrc=size=96x64:rate=30 -t 8 -pix_fmt yuv420p "$long"

cat <<EOF

Fixtures written to $directory

Configure and run the media tests with:

  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \\
    -DLUMAFRAMEVR_ASSEMBLY_VIDEO="$long"
  cmake --build build --parallel 2
  LUMAFRAMEVR_TEST_VIDEO="$short" ctest --test-dir build --output-on-failure

EOF
