#!/bin/sh
set -eu
if ! command -v python3 >/dev/null 2>&1; then
    printf '%s\n' 'Update failed: python3 is required' >&2
    exit 1
fi
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P) || exit 1
exec python3 -B "$root/update.py" "$@"
