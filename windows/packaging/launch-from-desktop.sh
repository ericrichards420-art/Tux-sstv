#!/bin/sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# Desktop launcher trust is maintained by GNOME's metadata service, so this
# step must run in the user's desktop session rather than the build sandbox.
desktop_dir=$(xdg-user-dir DESKTOP 2>/dev/null || printf '%s/Desktop' "$HOME")
if [ -f "$desktop_dir/OpenSSTV.desktop" ]; then
    chmod u+x "$desktop_dir/OpenSSTV.desktop"
    gio set -t string "$desktop_dir/OpenSSTV.desktop" metadata::trusted true 2>/dev/null || true
fi
# GNOME's settings service is accessible from the user's desktop session.
# Opening the application must still work if pinning is unavailable.
"$script_dir/pin-to-dock.py" || true
if [ -x "$HOME/.local/bin/tux-sstv" ]; then
    exec "$HOME/.local/bin/tux-sstv" "$@"
fi
exec "$HOME/.local/bin/opensstv" "$@"
