#!/usr/bin/env bash
# ZIP timestamps have no timezone. Clamp future-dated extracted inputs before
# CMake/Ninja so a manifest cannot remain older than its source dependencies.
set -euo pipefail
if [[ $# != 1 || ! -f $1/CMakeLists.txt ]]; then
    echo 'Usage: normalize-windows-source-times.sh TUX_SOURCE_DIRECTORY' >&2
    exit 1
fi
repo=$(cd -- "$1" && pwd)
mkdir -p "$repo/build-windows-logs"
reference=$(mktemp "$repo/build-windows-logs/source-time.XXXXXX")
future_list=$(mktemp "$repo/build-windows-logs/source-future.XXXXXX")
trap 'rm -f -- "$reference" "$future_list"' EXIT
manifest_hash=''
refresh_archive=false
stamp="$repo/build-windows/.tux-source-archive.sha256"
if [[ -f $repo/SOURCE_SHA256.json ]]; then
    manifest_hash=$(sha256sum -- "$repo/SOURCE_SHA256.json")
    manifest_hash=${manifest_hash%% *}
    previous_hash=''
    if [[ -f $stamp ]]; then previous_hash=$(cat -- "$stamp"); fi
    # Re-extracting an updated ZIP can preserve old file dates. Ensure existing
    # objects are rebuilt when the source archive changes, without deleting it.
    if [[ -f $repo/build-windows/CMakeCache.txt && $manifest_hash != "$previous_hash" ]]; then
        refresh_archive=true
    fi
fi
time_filter=()
if [[ $refresh_archive != true ]]; then time_filter=(-newer "$reference"); fi
# Never traverse build output, logs, evidence, Git metadata or symlinks.
find "$repo" -mindepth 1 \
    \( -type d \( -path "$repo/build*" -o -path "$repo/dist" -o -path "$repo/.git" -o -name __pycache__ \) -prune \) -o \
    \( -type f "${time_filter[@]}" -print0 \) > "$future_list"
count=0
while IFS= read -r -d '' file; do
    touch -m -r "$reference" -- "$file"
    count=$((count+1))
done < "$future_list"
if [[ $refresh_archive == true ]]; then
    echo "Updated source kit detected; refreshed $count source timestamp(s) for rebuilding."
else
    echo "Checked extracted source timestamps; corrected $count future-dated file(s)."
fi
if [[ -n $manifest_hash ]]; then
    mkdir -p "$repo/build-windows"
    printf '%s\n' "$manifest_hash" > "$stamp"
fi
