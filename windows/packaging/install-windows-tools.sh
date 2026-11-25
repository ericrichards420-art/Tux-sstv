#!/usr/bin/env bash
set -euo pipefail
if [[ ${MSYSTEM:-} != UCRT64 ]]; then
    echo 'Run this from the MSYS2 UCRT64 terminal.' >&2
    exit 1
fi
confirm=()
if [[ ${TUX_UNATTENDED:-0} == 1 ]]; then
    confirm=(--noconfirm)
else
    echo 'First finish MSYS2 updates using pacman -Syu, including any requested terminal restart.'
fi
echo 'Installing native Windows compiler, Qt, camera support and build tools:'
pacman "${confirm[@]}" -S --needed \
    make curl tar gzip \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-libusb \
    mingw-w64-ucrt-x86_64-qt6-base \
    mingw-w64-ucrt-x86_64-qt6-multimedia \
    mingw-w64-ucrt-x86_64-qt6-svg \
    mingw-w64-ucrt-x86_64-qt6-imageformats
echo 'Compiler and Qt build tools are installed.'
