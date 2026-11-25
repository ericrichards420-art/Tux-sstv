#!/usr/bin/python3
"""Pin the installed Tux-sstv launcher, preserving every existing favorite."""
import ast
import json
import os
from pathlib import Path
import subprocess
import sys


def favorites():
    result = subprocess.run(
        ["gsettings", "get", "org.gnome.shell", "favorite-apps"],
        check=True, text=True, capture_output=True,
    )
    value = result.stdout.strip()
    if value.startswith("@as "):
        value = value[4:]
    parsed = ast.literal_eval(value)
    if not isinstance(parsed, list) or not all(isinstance(x, str) for x in parsed):
        raise RuntimeError("Unexpected GNOME favorites format")
    return parsed


def main():
    launcher = Path.home() / ".local/share/applications/opensstv.desktop"
    if not launcher.is_file():
        raise RuntimeError("Install the Tux-sstv application launcher first")
    before = favorites()
    if "opensstv.desktop" in before:
        print("Tux-sstv is already pinned to the dock.")
        return
    state = Path(os.environ.get("XDG_STATE_HOME", Path.home() / ".local/state")) / "opensstv"
    state.mkdir(parents=True, exist_ok=True)
    backup = state / "dock-favorites-before.json"
    if not backup.exists():
        backup.write_text(json.dumps(before, indent=2) + "\n")
    after = before + ["opensstv.desktop"]
    result = subprocess.run(
        ["gsettings", "set", "org.gnome.shell", "favorite-apps", repr(after)],
        check=True, text=True, capture_output=True,
    )
    if favorites() != after:
        raise RuntimeError(result.stderr.strip() or "GNOME did not save the dock update")
    print("Tux-sstv pinned to the dock; existing favorites preserved.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Tux-sstv dock setup: {error}", file=sys.stderr)
        sys.exit(1)
