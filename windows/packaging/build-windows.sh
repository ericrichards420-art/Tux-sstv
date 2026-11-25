#!/usr/bin/env bash
set -euo pipefail
if [[ ${MSYSTEM:-} != UCRT64 ]]; then
    echo 'Run Build Windows.cmd or use the MSYS2 UCRT64 terminal.' >&2
    exit 1
fi
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd -- "$repo"
for program in cmake ninja gcc g++ make curl tar sha256sum windeployqt objdump find touch mktemp cygpath; do
    command -v "$program" >/dev/null || { echo "Missing $program. Run Install Windows Build Tools.cmd first." >&2; exit 1; }
done
windows_powershell="$(cygpath -u "${SYSTEMROOT:-${SystemRoot:-C:/Windows}}")/System32/WindowsPowerShell/v1.0/powershell.exe"
[[ -f $windows_powershell ]] || { echo 'Cannot find the built-in Windows PowerShell installer helper.' >&2; exit 1; }
version=$(sed -n 's/^project(TuxSSTV VERSION \([^ ]*\).*/\1/p' CMakeLists.txt)
[[ -n $version ]] || { echo 'Cannot read the application version.' >&2; exit 1; }
mkdir -p build-windows-logs
exec > >(tee build-windows-logs/build.log) 2>&1
hamlib_version=4.7.2
hamlib_sha=ae1fcf2dbc80ea0786ea8f047b09399c3f7737d1930442f61a031708ed33e88f
cache=${TUX_HAMLIB_CACHE:-/tmp/tux-sstv-hamlib-4.7.2-ucrt64}
[[ $cache != *' '* ]] || { echo 'Set TUX_HAMLIB_CACHE to a writable path without spaces.' >&2; exit 1; }
mkdir -p "$cache"
archive="$cache/hamlib-$hamlib_version.tar.gz"
prefix="$cache/install"
if [[ ! -f $archive ]]; then
    curl --fail --location --retry 3 "https://github.com/Hamlib/Hamlib/releases/download/$hamlib_version/hamlib-$hamlib_version.tar.gz" -o "$archive.download"
    mv -- "$archive.download" "$archive"
fi
printf '%s  %s\n' "$hamlib_sha" "$archive" | sha256sum --check -
if [[ ! -f $prefix/include/hamlib/rig.h || ! -f $prefix/bin/rigctl.exe ]]; then
    tar -xzf "$archive" -C "$cache"
    # Keep the official archive checksum intact; fix its Windows-only missing
    # assertion header in the extracted 4.7.2 sources before configure/make.
    bash "$repo/packaging/patch-hamlib-windows.sh" "$cache/hamlib-$hamlib_version"
    mkdir -p "$cache/build"
    (
        cd "$cache/build"
        CC=gcc CXX=g++ "$cache/hamlib-$hamlib_version/configure" --host=x86_64-w64-mingw32 --prefix="$prefix" --disable-static --enable-shared --disable-nls --without-readline
        make -j"${TUX_BUILD_JOBS:-4}"
        make install
    )
fi
export PATH="$prefix/bin:$PATH"
bash "$repo/packaging/normalize-windows-source-times.sh" "$repo"
cmake -S . -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON "-DHAMLIB_ROOT=$(cygpath -m "$prefix")"
if cmake --build build-windows --parallel "${TUX_BUILD_JOBS:-4}" 2>&1 | tee build-windows-logs/compile.log; then
    :
else
    build_status=$?
    if grep -Fq 'still dirty after' build-windows-logs/compile.log; then
        echo 'Ninja regeneration diagnostics (dry run; no commands are executed):'
        ninja -C build-windows -d explain -n 2>&1 | tail -n 35 || true
    fi
    exit "$build_status"
fi
if QT_QPA_PLATFORM=offscreen ctest --test-dir build-windows --output-on-failure | tee build-windows-logs/tests.log; then
    :
else
    test_status=$?
    echo
    echo 'Failed test details (also saved in build-windows-logs/tests.log):'
    # Keep the actual assertion visible below long lists of passing checks.
    sed -n '/FAIL!\|QFATAL\|FATAL/,+6p' build-windows-logs/tests.log
    exit "$test_status"
fi
stage="$repo/dist/windows/Tux-sstv"
if [[ -e $stage && ! -f $stage/.tux-windows-staging ]]; then
    echo "Refusing to replace an unrecognized folder: $stage" >&2
    exit 1
fi
if [[ -d $stage ]]; then rm -rf -- "$stage"; fi
mkdir -p "$stage"
touch "$stage/.tux-windows-staging"
cmake --install build-windows --prefix "$(cygpath -m "$stage")"
cp -- "$prefix/bin/rigctl.exe" "$stage/"
if [[ -f $prefix/bin/rigctld.exe ]]; then cp -- "$prefix/bin/rigctld.exe" "$stage/"; fi
while IFS= read -r -d '' library; do
    name=$(basename -- "$library")
    if [[ -f $stage/$name ]]; then
        source_hash=$(sha256sum -- "$library")
        target_hash=$(sha256sum -- "$stage/$name")
        [[ ${source_hash%% *} == "${target_hash%% *}" ]] || { echo "Conflicting Hamlib DLLs named $name" >&2; exit 1; }
    else cp -- "$library" "$stage/"; fi
done < <(find "$prefix" -type f -iname '*.dll' -print0)
windeployqt --release --no-translations --compiler-runtime --dir "$(cygpath -m "$stage")" "$(cygpath -m "$stage/tux-sstv.exe")"
bash "$repo/packaging/deploy-windows-platforms.sh" "$stage" /ucrt64/share/qt6/plugins
cmake "-DSTAGE=$(cygpath -m "$stage")" "-DQT_BIN=$(cygpath -m /ucrt64/bin)" "-DHAMLIB_BIN=$(cygpath -m "$prefix/bin")" -P cmake/WindowsRuntime.cmake
mkdir -p "$stage/licenses/Hamlib"
cp -- "$cache/hamlib-$hamlib_version/COPYING"* "$stage/licenses/Hamlib/"
# Include installed package notices for every bundled MSYS2 runtime DLL/plugin.
while IFS= read -r -d '' library; do
    installed="/ucrt64/bin/$(basename -- "$library")"
    [[ -f $installed ]] || continue
    owner=$(pacman -Qoq "$installed")
    [[ -n $owner ]] || continue
    while IFS= read -r notice; do
        if [[ $notice == */share/licenses/* && -f $notice ]]; then
            destination="$stage/licenses/dependencies/$owner/${notice#*/share/licenses/}"
            mkdir -p -- "$(dirname -- "$destination")"
            cp -- "$notice" "$destination"
        fi
    done < <(pacman -Qql "$owner")
done < <(find "$stage" -type f -iname '*.dll' -print0)
bash "$repo/packaging/check-windows-stage.sh" "$stage" "$repo/build-windows-logs"
cmake "-DSTAGE=$(cygpath -m "$stage")" "-DLOG_DIR=$(cygpath -m "$repo/build-windows-logs")" "-DAPP_VERSION=$version" -P cmake/VerifyWindowsBundle.cmake
cp -- build-windows-logs/tests.log "$stage/BUILD-TESTS.txt"
zip_name="Tux-sstv-$version-windows-x64-preview.zip"
(
    cd -- "$repo/dist/windows"
    cmake -E tar cf "$zip_name" --format=zip Tux-sstv
    sha256sum "$zip_name" > "$zip_name.sha256"
)
echo "Portable Windows preview: $repo/dist/windows/$zip_name"
echo 'Creating the Windows installer with a Start menu shortcut ...'
"$windows_powershell" -NoLogo -NoProfile -ExecutionPolicy Bypass \
    -File "$(cygpath -w "$repo/packaging/make-windows-installer.ps1")" -NoOpenFolder
echo 'Build complete. Test the portable folder on Windows before calling this a final release.'
