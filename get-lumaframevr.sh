#!/bin/sh
# LumaFrameVR, one line:
#
#   curl -fsSL https://raw.githubusercontent.com/GitHubStefan13/LumaFrameVR/main/get-lumaframevr.sh | sh
#
# Downloads the release that CMakeLists.txt declares, checks it against the checksum published
# beside it, and installs it silently, adding the Steam shortcut. No root, no Python, no prompts.
#
#   LUMAFRAMEVR_NO_STEAM=1   install without the Steam shortcut
#   LUMAFRAMEVR_VERSION=X.Y.Z  a specific release instead of the declared one
#   LUMAFRAMEVR_KEEP=1       keep the downloaded file next to the script's temporary directory
set -eu

repo="GitHubStefan13/LumaFrameVR"
branch="main"
api="https://api.github.com/repos/$repo"

note() { printf 'LumaFrameVR: %s\n' "$1"; }
fail() { note "$1" >&2; exit 1; }

[ "$(uname -m)" = "aarch64" ] ||
    note "warning: this build is for aarch64 (the Steam Frame), and this machine reports $(uname -m)" >&2
command -v curl >/dev/null 2>&1 || fail "curl is needed to download the release"

# The released version is the one the tree declares. Reading it here rather than writing it into
# this script means a release never requires an edit to it.
version=${LUMAFRAMEVR_VERSION:-}
if [ -z "$version" ]; then
    version=$(curl -fsSL "https://raw.githubusercontent.com/$repo/$branch/CMakeLists.txt" |
        sed -n 's/^project(LumaFrameVR VERSION \([^ )]*\).*/\1/p' | head -1)
fi
[ -n "$version" ] || fail "cannot read the version from CMakeLists.txt; is there a network?"

# The installer attached to that release, found by name so the installer build number is free to
# move without touching this file.
asset=$(curl -fsSL "$api/releases/tags/$version" 2>/dev/null | tr ',' '\n' |
    grep -o "https://[^\"]*-installer-$version-[0-9][0-9]*\.run" | head -1) || true
[ -n "${asset:-}" ] ||
    fail "release $version has no installer attached yet: https://github.com/$repo/releases"

name=${asset##*/}
directory=$(mktemp -d)
[ "${LUMAFRAMEVR_KEEP:-0}" = "1" ] || trap 'rm -rf "$directory"' EXIT INT TERM

note "downloading $name"
curl -fL --progress-bar -o "$directory/$name" "$asset" || fail "the download failed"
if curl -fL -o "$directory/$name.sha256" "$asset.sha256" 2>/dev/null && [ -s "$directory/$name.sha256" ]; then
    (cd "$directory" && sha256sum -c "$name.sha256") ||
        fail "the download does not match the published checksum; refusing to install it"
else
    note "no checksum was published for this release; the installer verifies its own payload"
fi

chmod +x "$directory/$name"
note "installing $version${LUMAFRAMEVR_NO_STEAM:+ without the Steam shortcut}"
if [ "${LUMAFRAMEVR_NO_STEAM:-0}" = "1" ]; then
    "$directory/$name" --install --no-steam --yes "$@"
else
    "$directory/$name" --install --yes "$@"
fi
note "done; LumaFrameVR $version is installed"
