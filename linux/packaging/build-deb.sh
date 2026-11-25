#!/bin/sh
set -eu
umask 022
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake -S "$project_dir" -B "$project_dir/build-package" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$project_dir/build-package" -j 4
cpack --config "$project_dir/build-package/CPackConfig.cmake" -B "$project_dir/dist"
