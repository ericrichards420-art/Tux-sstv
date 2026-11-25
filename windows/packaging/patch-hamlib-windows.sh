#!/usr/bin/env bash
# Hamlib 4.7.2's Windows timer uses assert() without including <assert.h>.
# Apply this only after verifying and extracting the pinned source archive.
set -euo pipefail
if [[ $# != 1 ]]; then
    echo 'Usage: patch-hamlib-windows.sh HAMLIB_SOURCE_DIRECTORY' >&2
    exit 1
fi
source_file="$1/lib/precise_time.c"
if [[ ! -f $source_file ]]; then
    echo "Cannot apply Hamlib Windows header fix: missing $source_file" >&2
    exit 1
fi
windows_header='^[[:space:]]*#[[:space:]]*include[[:space:]]*<windows[.]h>'
if [[ $(grep -Ec "$windows_header" "$source_file") != 1 ]] ||
   ! grep -Eq 'assert[[:space:]]*\([[:space:]]*has_qpc[[:space:]]*\)' "$source_file"; then
    echo 'Cannot apply Hamlib Windows header fix: expected 4.7.2 timer source was not found.' >&2
    exit 1
fi
added_header='#include <assert.h> /* Tux-sstv: Hamlib 4.7.2 Windows assert fix. */'
if grep -Fxq "$added_header" "$source_file"; then
    echo 'Hamlib 4.7.2 Windows assertion header fix is already applied.'
    exit 0
fi
sed -i '/^[[:space:]]*#[[:space:]]*include[[:space:]]*<windows[.]h>/a\#include <assert.h> /* Tux-sstv: Hamlib 4.7.2 Windows assert fix. */' "$source_file"
echo 'Applied Hamlib 4.7.2 Windows assertion header fix.'
