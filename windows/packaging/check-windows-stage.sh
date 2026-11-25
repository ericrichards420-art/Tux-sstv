#!/usr/bin/env bash
# Run metadata checks without opening the radio GUI or using installed plugins.
set -euo pipefail
if [[ $# != 2 ]]; then
    echo 'Usage: check-windows-stage.sh STAGE_DIRECTORY LOG_DIRECTORY' >&2
    exit 1
fi
stage=$1
logs=$2
for file in tux-sstv.exe rigctl.exe qt.conf platforms/qwindows.dll platforms/qoffscreen.dll; do
    [[ -f $stage/$file ]] || { echo "Missing bundle component before startup: $file" >&2; exit 1; }
done
mkdir -p "$logs"
unset QT_PLUGIN_PATH
export QT_QPA_PLATFORM=offscreen
export QT_QPA_PLATFORM_PLUGIN_PATH
QT_QPA_PLATFORM_PLUGIN_PATH=$(cygpath -m "$stage/platforms")
export QT_DEBUG_PLUGINS=1 QT_LOGGING_TO_CONSOLE=1
check_program(){
    local output=$1 diagnostic=$2
    shift 2
    if "$@" > "$output" 2> "$diagnostic"; then
        :
    else
        local status=$?
        echo "Staged program check failed; Qt diagnostics: $diagnostic" >&2
        tail -n 70 "$diagnostic" >&2
        return "$status"
    fi
}
check_program "$logs/staged-version.txt" "$logs/staged-version-plugins.log" "$stage/tux-sstv.exe" --version
check_program "$logs/staged-modes.json" "$logs/staged-modes-plugins.log" "$stage/tux-sstv.exe" --list-modes
"$stage/rigctl.exe" -l > "$logs/staged-radio-models.txt"
