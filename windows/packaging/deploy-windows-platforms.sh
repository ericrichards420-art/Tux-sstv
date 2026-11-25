#!/usr/bin/env bash
# windeployqt normally deploys qwindows, but CLI image operations also need
# qoffscreen. Both must come from the same Qt installation as the program.
set -euo pipefail
if [[ $# != 2 || ! -f $1/.tux-windows-staging ]]; then
    echo 'Usage: deploy-windows-platforms.sh OWNED_STAGE_DIRECTORY QT_PLUGIN_DIRECTORY' >&2
    exit 1
fi
stage=$1
qt_plugins=$2
command -v sha256sum >/dev/null || { echo 'Missing sha256sum. Run Install Windows Build Tools.cmd first.' >&2; exit 1; }
for plugin in qwindows qoffscreen; do
    source="$qt_plugins/platforms/$plugin.dll"
    target="$stage/platforms/$plugin.dll"
    [[ -f $source ]] || { echo "Missing Qt platform plugin: $source. Reinstall the UCRT64 qt6-base package." >&2; exit 1; }
    if [[ -f $target ]]; then
        source_hash=$(sha256sum -- "$source")
        target_hash=$(sha256sum -- "$target")
        if [[ ${source_hash%% *} != "${target_hash%% *}" ]]; then
            echo "Qt platform plugin differs from the active Qt installation: $target" >&2
            exit 1
        fi
    fi
done
mkdir -p "$stage/platforms"
for plugin in qwindows qoffscreen; do
    if [[ ! -f $stage/platforms/$plugin.dll ]]; then
        cp -- "$qt_plugins/platforms/$plugin.dll" "$stage/platforms/"
    fi
done
# The portable layout keeps plugin categories beside the executable.
printf '[Paths]\nPrefix=.\nPlugins=.\n' > "$stage/qt.conf"
echo 'Deployed matching Windows/offscreen Qt platform plugins and portable Qt paths.'
