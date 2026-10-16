#!/usr/bin/env bash
# Generate the media fixtures the acceptance tests read.
#
# The tests skip when a fixture is absent, so this is optional: run it when you
# want the media tests to actually execute. It needs the ffmpeg CLI, and the
# AV1 and ten-bit clips need an encoder for each; without one it says so and
# writes the rest.
#
# Four clips, because the tests use four different contracts:
#   video-96x64-2s.mp4      the short media contract (LUMAFRAMEVR_TEST_VIDEO)
#   video-8s.mp4            the eight-second assembly fixture (LUMAFRAMEVR_ASSEMBLY_VIDEO)
#   video-av1-96x64-2s.mkv  AV1, which this player decodes in software where
#                           libdav1d is not part of the FFmpeg it links
#   video-10bit-96x64-2s.mkv  ten-bit 4:2:0, which the software path packs to P010
#   video-10bit-hdr-96x64-2s.mkv  ten-bit PQ/BT.2020, so the colour metadata the
#                           tone-mapping shader is driven from is exercised too
#
# The AV1, ten-bit and short clips are the same testsrc pattern, so the ten-bit
# decode can be compared against the eight-bit one in the codec_support test:
# "it decoded" is not the same as "the picture is right".
#
# Usage: scripts/make-test-fixtures.sh [output-directory]
set -euo pipefail

directory=${1:-$PWD/test-fixtures}
mkdir -p -- "$directory"

if ! command -v ffmpeg >/dev/null 2>&1; then
    printf 'ffmpeg is required; install it first (for example: sudo apt install ffmpeg)\n' >&2
    exit 1
fi

encoder_available() {
    ffmpeg -hide_banner -encoders 2>/dev/null | awk '{print $2}' | grep -qx "$1"
}

short=$directory/video-96x64-2s.mp4
long=$directory/video-8s.mp4
av1=$directory/video-av1-96x64-2s.mkv
tenbit=$directory/video-10bit-96x64-2s.mkv
hdr=$directory/video-10bit-hdr-96x64-2s.mkv

printf 'Writing %s\n' "$short"
ffmpeg -y -loglevel error -f lavfi -i testsrc=size=96x64:rate=30 -t 2 -pix_fmt yuv420p "$short"

printf 'Writing %s\n' "$long"
ffmpeg -y -loglevel error -f lavfi -i testsrc=size=96x64:rate=30 -t 8 -pix_fmt yuv420p "$long"

if encoder_available libaom-av1; then
    printf 'Writing %s\n' "$av1"
    ffmpeg -y -loglevel error -f lavfi -i testsrc=size=96x64:rate=30 -t 2 -pix_fmt yuv420p \
        -c:v libaom-av1 -cpu-used 8 -crf 40 -b:v 0 "$av1"
else
    printf 'No libaom-av1 encoder: %s not written, the codec_support test will skip AV1\n' "$av1" >&2
fi

if encoder_available libx265; then
    printf 'Writing %s\n' "$tenbit"
    ffmpeg -y -loglevel error -f lavfi -i testsrc=size=96x64:rate=30 -t 2 -pix_fmt yuv420p10le \
        -c:v libx265 -x265-params log-level=none "$tenbit"
    printf 'Writing %s\n' "$hdr"
    ffmpeg -y -loglevel error -f lavfi -i testsrc=size=96x64:rate=30 -t 2 -pix_fmt yuv420p10le \
        -c:v libx265 -x265-params log-level=none:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc "$hdr"
else
    if encoder_available libvpx-vp9; then
        printf 'No libx265 encoder: writing %s as VP9 profile 2 instead\n' "$tenbit"
        ffmpeg -y -loglevel error -f lavfi -i testsrc=size=96x64:rate=30 -t 2 -pix_fmt yuv420p10le \
            -c:v libvpx-vp9 -profile:v 2 -crf 40 -b:v 0 "$tenbit"
        printf 'No libx265 encoder: writing %s as VP9 profile 2 instead\n' "$hdr"
        ffmpeg -y -loglevel error -f lavfi -i testsrc=size=96x64:rate=30 -t 2 -pix_fmt yuv420p10le \
            -color_primaries bt2020 -color_trc smpte2084 -colorspace bt2020nc \
            -c:v libvpx-vp9 -profile:v 2 -crf 40 -b:v 0 "$hdr"
    else
        printf 'Neither libx265 nor libvpx-vp9: %s and %s not written, the codec_support test will skip ten-bit\n' "$tenbit" "$hdr" >&2
    fi
fi

cat <<EOF

Fixtures written to $directory

Configure and run the media tests with:

  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \\
    -DLUMAFRAMEVR_ASSEMBLY_VIDEO="$long" \\
    -DLUMAFRAMEVR_CODEC_FIXTURES="$directory"
  cmake --build build --parallel 2
  LUMAFRAMEVR_TEST_VIDEO="$short" ctest --test-dir build --output-on-failure

EOF
