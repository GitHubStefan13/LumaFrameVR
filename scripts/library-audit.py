#!/usr/bin/env python3
"""Record which libraries the player needs and where each one is expected to come from.

The payload ships no libraries the ELF links directly. The player links the FFmpeg the frame
already has and takes the loaders from the system or SteamVR, so every dependency here is
either provided by the system or the runtime, or it is missing and therefore a defect. The
audit is written next to the player so the decision can be checked from the artifact itself,
and `shipped_in_payload` stays in the document as an empty object: nothing takes that route.

Reads the ELF dynamic section, so it needs readelf. Refuses to write an empty audit: an
audit that lists nothing is worse than none, because it looks like a check that passed.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

NOTES = {
    'libavcodec.so.61': "the frame's own FFmpeg decoder; the player links it and the payload "
                        "ships no copy",
    'libavformat.so.61': "the frame's own FFmpeg demuxer; provided by the system, not the payload",
    'libavutil.so.59': "the frame's own FFmpeg support library; provided by the system",
    'libswresample.so.5': "the frame's own FFmpeg resampler; provided by the system",
    'libvulkan.so.1': 'system loader, matched to the freedreno driver; a loader from the '
                      'package crashed inside the driver',
    'libopenxr_loader.so.1': 'SteamVR loader, found through the launcher path',
    'libopenvr_api.so': 'shipped in runtime/, opened by name from the package root',
    'libjsoncpp.so.25': 'dropped: only the package OpenXR loader needed it',
}


def needed_libraries(binary):
    readelf = shutil.which('readelf')
    if readelf is None:
        raise SystemExit('library-audit.py: readelf is not on PATH')
    listing = subprocess.run([readelf, '-d', binary], capture_output=True, text=True, check=True)
    names = []
    for line in listing.stdout.splitlines():
        if 'NEEDED' in line and '[' in line:
            names.append(line.split('[', 1)[1].rstrip(']'))
    if not names:
        raise SystemExit('library-audit.py: readelf reported no dependencies for ' + binary)
    return names


def main():
    parser = argparse.ArgumentParser(description='Audit the libraries the player needs')
    parser.add_argument('--binary', required=True)
    parser.add_argument('--output', required=True)
    options = parser.parse_args()
    names = needed_libraries(options.binary)
    # Nothing is classified as shipped. The payload carries no library the ELF links directly:
    # the FFmpeg the player needs is the frame's own, and runtime/libopenvr_api.so is opened by
    # name rather than linked, so it never appears in the dynamic section either.
    shipped, system = {}, list(names)
    document = {
        'binary': options.binary.rsplit('/', 1)[-1],
        'needed_count': len(names),
        'shipped_in_payload': shipped,
        'provided_by_system_or_runtime': sorted(system),
        'notes': NOTES,
    }
    # The installer chain clears build/dist just before this runs, and the caller may point
    # the output anywhere, so the parent directory is this script's responsibility.
    os.makedirs(os.path.dirname(os.path.abspath(options.output)), exist_ok=True)
    with open(options.output, 'w', encoding='utf-8') as handle:
        json.dump(document, handle, indent=2)
        handle.write('\n')
    print('library-audit.py: %d dependencies, %d shipped, %d from the frame'
          % (len(names), len(shipped), len(system)))


if __name__ == '__main__':
    main()
