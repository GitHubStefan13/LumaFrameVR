#!/usr/bin/env bash
# Initialise the environment and data a device test session needs.
#
#   cpp/tests/init-test-environment.sh [directory]
#
# [directory] defaults to $HOME/Videos/VRTestVideos. It is filled with:
#
#   * the two small clips the acceptance suite reads, written from ffmpeg's own
#     synthetic sources by scripts/make-test-fixtures.sh;
#   * a sample set for the device, so that a projection or codec change can be
#     told apart from a decode fault.
#
# The sample set is deliberately small - about 75 MB, and it should stay there -
# and every file in it exercises something the player has code for: VR 180 in mono
# and top-bottom stereo, H.264, 4K HEVC and 4K VP9, the MP4, WebM and MKV containers,
# Opus audio, and a stream with no audio at all. Formats the player refuses before it
# opens a decoder (AVS3, VVC, ProRes, 12-bit, and the ten-bit layouts the software
# packer has no shift for) are not downloaded: they test the refusal, not the
# pipeline. The 360-degree clips this set once carried come from a dataset of tens of
# gigabytes, which is far outside the budget, so a 360 projection sample has to be
# supplied by hand.
#
# Two clips are generated rather than downloaded, by make-test-fixtures.sh: the two
# the suite reads, and a ten-bit one - which is the sample to play first when the
# ten-bit path is checked on the frame.
#
# AV1 is in the table: the frame's FFmpeg carries libdav1d and decodes AV1 in
# software, which the player reports in its own decoder line. (The AArch64 SDK copy
# taken off the frame says otherwise - CONFIG_LIBDAV1D_DECODER 0 and no dav1d strings
# - so the copy is not authoritative about the device, and the probe at run time is.)
#
# Downloads resume, run JOBS at a time and are skipped when the file is already
# there, so a second run is cheap.

set -euo pipefail

declare -A downloads=(
    # VR 180: the projection the player resolves beyond flat, in mono and top-bottom stereo.
    [glyphew-uv-180.mp4]="https://thanford.com/glyphew/clips/uv_180.mp4"
    [glyphew-uv-180-tb.mp4]="https://thanford.com/glyphew/clips/uv_180_tb.mp4"

    # One clip per decodable codec: H.264 on the software path, HEVC and VP9 on the VPU, and AV1,
    # which the frame's FFmpeg decodes with libdav1d.
    [elecard-h264-theater-square-1080.mp4]="https://www.elecard.com/storage/video/TheaterSquare_1920x1080.mp4"
    [elecard-hevc-tsu-2160.mp4]="https://www.elecard.com/storage/video/TSU_3840x2160.mp4"
    [elecard-vp9-ushaika-river-2160.webm]="https://www.elecard.com/storage/video/UshaikaRiverEmb_3840x2160.webm"
    [elecard-av1-city-hall-1080.webm]="https://www.elecard.com/storage/video/CityHall_1920x1080.webm"

    # Opus audio in WebM, and the silent-media path that has its own code branch.
    [remotion-video-vp9-opus.webm]="https://remotion.media/video-vp9-opus.webm"
    [remotion-video-none.mp4]="https://remotion.media/video-none.mp4"
)

require_space() {
    local available
    available=$(df -B1 --output=avail "$directory" | tail -n 1)
    if (( available < $1 )); then
        printf 'Need %s bytes free in %s\n' "$1" "$directory" >&2
        exit 1
    fi
}

process_file() {
    local name=$1 file="$directory/$1"
    if [[ -e "$file" ]]; then
        printf 'Skipping %s\n' "$name"
    else
        printf 'Downloading %s\n' "$name"
        if ! curl --silent --show-error --fail --location --retry 3 --continue-at - \
            --output "$file.part" "$2"; then
            printf 'Skipping failed download: %s\n' "$name" >&2
            return
        fi
        mv -- "$file.part" "$file"
    fi
}

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
directory=${1:-$HOME/Videos/VRTestVideos}
jobs=${JOBS:-4}
if ! [[ "$jobs" =~ ^[1-9][0-9]*$ ]]; then
    printf 'JOBS must be a positive integer\n' >&2
    exit 1
fi
mkdir -p -- "$directory"
# Generous rather than exact: the largest single file here is about 50 MB, so this
# guards against a full filesystem rather than measuring the set.
require_space 1000000000

if ! "$root/scripts/make-test-fixtures.sh" "$directory"; then
    printf 'Skipping the generated fixtures: scripts/make-test-fixtures.sh failed (is ffmpeg installed?)\n' >&2
fi

export directory
export -f process_file require_space
for name in "${!downloads[@]}"; do
    printf '%s\0%s\0' "$name" "${downloads[$name]}"
done | xargs -0 -r -n 2 -P "$jobs" bash -euo pipefail -c 'process_file "$@"' _

printf '\nSample set in %s\n' "$directory"
printf 'For a host ctest run:\n'
printf '  LUMAFRAMEVR_TEST_VIDEO=%s/video-96x64-2s.mp4\n' "$directory"
printf '  LUMAFRAMEVR_ASSEMBLY_VIDEO=%s/video-8s.mp4\n' "$directory"
