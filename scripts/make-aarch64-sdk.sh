#!/usr/bin/env bash
# Assemble a local AArch64 SDK from the Steam Frame, for cross-compiling here.
#
# The cross compiler (gcc-aarch64-linux-gnu) provides the compiler and libc, but
# not the target libraries the player links against (Vulkan, OpenXR, FFmpeg,
# PulseAudio). This copies those libraries and their full dependency closure off
# the frame, read-only, into ~/lumaframevr-sdk. Nothing is installed on the frame.
#
# The headers are architecture independent and come from the host's development
# packages, so only the libraries are copied.
#
# Usage: scripts/make-aarch64-sdk.sh [destination]
set -euo pipefail

destination=${1:-$HOME/lumaframevr-sdk}
frame=${LUMAFRAMEVR_FRAME:-steamos@frame}
ssh_key=${LUMAFRAMEVR_SSH_KEY:-$HOME/.ssh/lumaframe}
ssh_command="ssh -i $ssh_key"

# The OpenXR loader lives under a Proton directory whose name contains a space,
# so it is copied on its own rather than through the closure list.
openxr_loader="/home/steamos/.local/share/Steam/steamapps/common/Proton 11.0 (ARM64)/files/lib/aarch64-linux-gnu/libopenxr_loader.so.1.1.36"

# The libraries the player links against. The frame's /usr/lib/libopenxr_loader.a
# is a stub with no symbols, so the real loader comes from the Proton runtime.
frame_libraries=(
    /usr/lib/libvulkan.so.1
    /usr/lib/libavcodec.so.61
    /usr/lib/libavformat.so.61
    /usr/lib/libavutil.so.59
    /usr/lib/libswresample.so.5
    /usr/lib/libpulse.so.0
)

mkdir -p "$destination/lib" "$destination/pkgconfig" "$destination/include"

printf 'Collecting the library closure from %s ...\n' "$frame"
$ssh_command "$frame" "bash -s" <<REMOTE >"$destination/.closure"
set -e
declare -A seen
queue=($(printf '%q ' "${frame_libraries[@]}"))
while [ \${#queue[@]} -gt 0 ]; do
  lib=\${queue[0]}; queue=(\${queue[@]:1})
  [ -n "\${seen[\$lib]:-}" ] && continue
  seen[\$lib]=1
  for dep in \$(ldd "\$lib" 2>/dev/null | awk '/=>/ {print \$3}'); do
    [ -f "\$dep" ] && queue+=("\$dep")
  done
done
for lib in "\${!seen[@]}"; do echo "\$lib"; done
REMOTE

# rsync --files-from cannot carry paths with spaces, so drop any such line.
grep -v '[[:space:]]' "$destination/.closure" | sed 's|^/||' >"$destination/.closure.rel"
printf 'Copying %s libraries ...\n' "$(wc -l <"$destination/.closure.rel")"
rsync -azL --files-from="$destination/.closure.rel" -e "$ssh_command" "$frame:/" "$destination/lib/"
rm -f "$destination/.closure" "$destination/.closure.rel"

# Flatten the nested paths rsync created.
find "$destination/lib" -mindepth 2 -name '*.so*' -type f -exec mv -f {} "$destination/lib/" \; 2>/dev/null || true
find "$destination/lib" -mindepth 1 -type d -exec rm -rf {} + 2>/dev/null || true

# The OpenXR loader, copied on its own because of the space in its path.
$ssh_command "$frame" "cat '$openxr_loader'" >"$destination/lib/libopenxr_loader.so.1"

# Linker names for the libraries the build links against directly.
for pair in libvulkan.so.1:libvulkan.so libopenxr_loader.so.1:libopenxr_loader.so \
            libavcodec.so.61:libavcodec.so libavformat.so.61:libavformat.so \
            libavutil.so.59:libavutil.so libswresample.so.5:libswresample.so \
            libpulse.so.0:libpulse.so; do
    target=${pair%%:*}; link=${pair##*:}
    [ -f "$destination/lib/$target" ] && ln -sf "$target" "$destination/lib/$link"
done

# Headers are architecture independent; take them from the host development
# packages. FFmpeg headers live under the multiarch include directory.
for directory in vulkan openxr pulse; do
    [ -d "/usr/include/$directory" ] && cp -r "/usr/include/$directory" "$destination/include/"
done
for directory in libavcodec libavformat libavutil libswresample; do
    [ -d "/usr/include/x86_64-linux-gnu/$directory" ] &&
        cp -r "/usr/include/x86_64-linux-gnu/$directory" "$destination/include/"
done

write_pc() {
    local name=$1 version=$2 libs=$3
    cat >"$destination/pkgconfig/$name.pc" <<PC
prefix=$destination
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: $name
Description: $name (aarch64, from the Steam Frame)
Version: $version
Libs: -L\${libdir} $libs
Cflags: -I\${includedir}
PC
}

write_pc vulkan 1.4.309 -lvulkan
write_pc openxr 1.1 -lopenxr_loader
write_pc libavformat 61.1.100 -lavformat
write_pc libavcodec 61.3.100 -lavcodec
write_pc libavutil 59.8.100 -lavutil
write_pc libswresample 5.1.100 -lswresample
write_pc libpulse 17.0 -lpulse

printf 'SDK ready at %s (%s libraries)\n' "$destination" "$(find "$destination/lib" -name '*.so*' | wc -l)"
printf 'Configure with: cmake --preset frame-aarch64-gnu\n'

# The copy above takes runtime libraries, and a runtime library ships libfoo.so.1 with no libfoo.so.
# The distribution linker wants the bare name, so the development names are linked to the runtime
# files. Without this the SDK is complete but not linkable, which cost a link failure here.
printf 'Linking the development names for the copied libraries ...\n'
for library in "$destination"/lib/*.so.*; do
    [ -e "$library" ] || continue
    name=${library%.so.*}.so
    [ -e "$name" ] || ln -s "$(basename "$library")" "$name"
done
