#!/bin/sh
# LumaFrameVR launcher. Sets up the runtime environment and starts the player.
#
# This file is the single place where the runtime environment is defined, so
# starting the player from a desktop entry, from Steam, from this script or from
# a terminal all behave the same way. In particular DISABLE_GAMESCOPE_WSI keeps
# the Vulkan swapchain path off the gamescope WSI layer, which is what the Steam
# Deck session needs.
set -eu
directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
state=${XDG_STATE_HOME:-"$HOME/.local/state"}/lumaframevr
config=${XDG_CONFIG_HOME:-"$HOME/.config"}/lumaframevr
cache=${XDG_CACHE_HOME:-"$HOME/.cache"}/lumaframevr
log=${LUMAFRAMEVR_LOG_FILE:-"$state/startup.log"}
mkdir -p -- "$(dirname -- "$log")"
printf 'LumaFrameVR startup log: %s\n' "$log"
exec >>"$log" 2>&1
printf '\n--- LumaFrameVR launch ---\n'
date -Is
printf 'Package=%s SteamAppId=%s SteamGameId=%s ENABLE_GAMESCOPE_WSI=%s\n' \
    "$directory" "${SteamAppId:-unset}" "${SteamGameId:-unset}" "${ENABLE_GAMESCOPE_WSI:-unset}"
# A Steam session exports its own LD_LIBRARY_PATH, which can shadow the frame's
# libraries. Record it, then rebuild the path with our directories first and the
# inherited value after, so our libraries win but the session's own paths (the
# Proton runtime, which holds the OpenXR loader) stay reachable.
printf 'InheritedLD_LIBRARY_PATH=%s\n' "${LD_LIBRARY_PATH:-unset}"
video_nodes=
for node in /dev/video*; do
    [ -e "$node" ] || continue
    if [ -r "$node" ] && [ -w "$node" ]; then
        printf 'VPU %s readable\n' "$node"
    else
        printf 'VPU %s NOT readable, hardware decoding will fall back to software\n' "$node"
    fi
    video_nodes="$video_nodes $node"
done
printf 'VPU nodes:%s\n' "${video_nodes:- none}"
inherited_library_path=${LD_LIBRARY_PATH:-}
unset LD_LIBRARY_PATH
# The payload ships no FFmpeg: the player links the frame's own libav* and finds them on the
# system loader path, so only our runtime/ and the session's own directories are added here.
library_path=""
for candidate in "$directory/runtime" \
                 /opt/steamvr/bin/linuxarm64 /opt/steamvr/lib/linuxarm64 \
                 "$HOME/.steam/steam/steamapps/common/SteamVR/bin/linuxarm64" \
                 "$HOME/.local/share/Steam/steamapps/common/SteamVR/bin/linuxarm64"; do
    [ -d "$candidate" ] && library_path="$library_path${library_path:+:}$candidate"
done
export LD_LIBRARY_PATH="$library_path${inherited_library_path:+:$inherited_library_path}"
# Which directories the loaders were found in, so the log shows whether the system or the
# package supplied them.
printf 'LibraryDirs=%s\n' "${library_path:-none}"
export LUMAFRAMEVR_PACKAGE_ROOT="$directory"
# Session environment the player and its libraries expect. A Steam or desktop
# launch may not provide all of these, so fill in the standard locations.
export TMPDIR="${TMPDIR:-$state/tmp}"
export TMP="${TMP:-$TMPDIR}"
export TEMP="${TEMP:-$TMPDIR}"
mkdir -p -- "$TMPDIR"
export MESA_SHADER_CACHE_DIR="${MESA_SHADER_CACHE_DIR:-$cache/mesa}"
export MESA_GLSL_CACHE_DIR="${MESA_GLSL_CACHE_DIR:-$cache/mesa}"
mkdir -p -- "$MESA_SHADER_CACHE_DIR"
if [ -z "${XDG_RUNTIME_DIR:-}" ] && [ -d "/run/user/$(id -u)" ]; then
    export XDG_RUNTIME_DIR="/run/user/$(id -u)"
fi
if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    export DISPLAY=:0
fi
printf 'XDG_RUNTIME_DIR=%s TMPDIR=%s DISPLAY=%s WAYLAND_DISPLAY=%s\n' \
    "${XDG_RUNTIME_DIR:-unset}" "$TMPDIR" "${DISPLAY:-unset}" "${WAYLAND_DISPLAY:-unset}"
# The VPU is what makes hardware decoding possible. Log its state on every
# launch, so a denied or missing node is visible in the log rather than only as a
# decoder failure later.
vpu_nodes=$(ls /dev/video* 2>/dev/null || true)
if [ -z "$vpu_nodes" ]; then
    printf 'VPU nodes: none (no /dev/video*; hardware decoding will fail)\n'
else
    for node in $vpu_nodes; do
        if [ -r "$node" ] && [ -w "$node" ]; then
            printf 'VPU %s readable\n' "$node"
        else
            printf 'VPU %s NOT readable, hardware decoding will fail\n' "$node"
        fi
    done
fi
# Steam sets ENABLE_GAMESCOPE_WSI=1 when it launches a game on a gamescope
# session (the Steam Frame and Steam Deck). Only disable the gamescope WSI layer
# when Steam has not asked for it, so the two never contradict each other.
if [ -z "${ENABLE_GAMESCOPE_WSI:-}" ]; then
    export DISABLE_GAMESCOPE_WSI=1
fi
# SteamVR reads its path registry and writes its logs. Only point these at our
# own locations when the registry already exists there; otherwise leave SteamVR
# to its own defaults, because an override that names a missing file makes the
# OpenXR loader skip the runtime ("negotiation failed").
if [ -n "${VR_PATHREG_OVERRIDE:-}" ]; then
    :
elif [ -f "$config/openvrpaths.vrpath" ]; then
    export VR_PATHREG_OVERRIDE="$config/openvrpaths.vrpath"
elif [ -f "$HOME/.config/openvr/openvrpaths.vrpath" ]; then
    export VR_PATHREG_OVERRIDE="$HOME/.config/openvr/openvrpaths.vrpath"
fi
if [ -n "${VR_LOG_PATH:-}" ]; then
    :
elif [ -d "$state/steamvr" ]; then
    export VR_LOG_PATH="$state/steamvr"
fi
if [ -z "${XR_RUNTIME_JSON:-}" ] && [ -f /opt/steamvr/steamxr_linuxarm64.json ]; then
    export XR_RUNTIME_JSON=/opt/steamvr/steamxr_linuxarm64.json
fi
printf 'XR_RUNTIME_JSON=%s VR_PATHREG_OVERRIDE=%s\n' \
    "${XR_RUNTIME_JSON:-system default}" "${VR_PATHREG_OVERRIDE:-steamvr default}"
ulimit -c 0
cd -- "$directory"
exec "$directory/lumaframevr" "$@"
