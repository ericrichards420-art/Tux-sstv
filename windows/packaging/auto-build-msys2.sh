#!/usr/bin/env bash
set -euo pipefail
if [[ ${MSYSTEM:-} != UCRT64 ]]; then
    echo 'This helper must run in MSYS2 UCRT64.' >&2
    exit 1
fi
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd -- "$repo"
mkdir -p build-windows-logs
exec > >(tee -a build-windows-logs/automatic-build.log) 2>&1
case ${1:-} in
    initialize)
        echo 'MSYS2 login environment initialized.'
        command -v pacman >/dev/null
        ;;
    update)
        echo 'Updating MSYS2 packages; the Windows controller opens a fresh shell after core updates.'
        pacman --noconfirm -Syu
        ;;
    build)
        export TUX_UNATTENDED=1
        bash packaging/install-windows-tools.sh
        bash packaging/build-windows.sh
        ;;
    *)
        echo 'Expected initialize, update, or build.' >&2
        exit 1
        ;;
esac
