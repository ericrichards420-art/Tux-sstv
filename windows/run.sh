#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ ! -x "$project_dir/build/tux-sstv" ]; then
    cmake -S "$project_dir" -B "$project_dir/build" -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build "$project_dir/build" -j 4
fi
exec "$project_dir/build/tux-sstv" "$@"
