#!/usr/bin/env bash
# Asset-free builds for GitHub-hosted runners; not a replacement for build.sh.
set -euo pipefail
cd "$(dirname "$0")/.."
target=${1:?target required}
version=${2:?version required}

# Release runners must use a clean checkout, never a local game installation.
if [ -e data.pck ] || [ -L data.pck ] || [ -e romfs/data.pck ] || [ -L romfs/data.pck ]; then
    echo 'Release builds require a checkout without data.pck.' >&2
    exit 1
fi
common=(-DBT3D_FETCH_RAYLIB=ON -DBT3D_MIDI_SRC=/nonexistent/m1.dat)
package() {
    python3 scripts/package-release.py "$1" "$version" "$(git rev-parse HEAD)" "$2"
}
case "$target" in
    macos|linux)
        if [ "$target" = macos ]; then
            common+=(-DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0)
        fi
        cmake -S . -B build/ci "${common[@]}" -DBT3D_BUILD_TESTS=ON
        cmake --build build/ci --parallel 3
        ctest --test-dir build/ci --output-on-failure
        if [ "$target" = macos ]; then
            BUILD_DIR=build/ci DATA_PCK_SRC=/nonexistent/data.pck bash scripts/package-mac-app.sh
            /usr/libexec/PlistBuddy -c 'Set :LSMinimumSystemVersion 11.0' 'dist/macos/Bad Toys 3D.app/Contents/Info.plist'
            codesign --force --deep --sign - 'dist/macos/Bad Toys 3D.app'
            codesign --verify --deep --strict 'dist/macos/Bad Toys 3D.app'
            for arch in arm64 x86_64; do
                lipo build/ci/bt3d_raylib -verify_arch "$arch"
            done
            package macos-universal build/ci/_deps/raylib-src
        else
            package linux-x86_64 build/ci/_deps/raylib-src
        fi
        ;;
    cross)
        docker run --rm -v "$PWD:/work" -w /work bt3d-ci:latest bash -ec '
            cmake -S . -B build/windows -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/zig-windows-toolchain.cmake -DBT3D_FETCH_RAYLIB=ON -DBT3D_MIDI_SRC=/nonexistent/m1.dat
            cmake --build build/windows --parallel 3
            cmake -S . -B build/switch -DCMAKE_TOOLCHAIN_FILE=cmake/switch-toolchain.cmake -DBT3D_PLATFORM_SWITCH=ON -DBT3D_EMBED_ROMFS=OFF -DBT3D_MIDI_SRC=/nonexistent/m1.dat
            cmake --build build/switch --parallel 3
        '
        package windows-x86_64 build/windows/_deps/raylib-src
        package switch build/windows/_deps/raylib-src
        ;;
    web)
        docker run --rm -v "$PWD:/work" -w /work bt3d-ci:latest bash -ec '
            emcmake cmake -S . -B build/web -G Ninja -DBT3D_PLATFORM_WEB=ON -DBT3D_MIDI_SRC=/nonexistent/m1.dat
            cmake --build build/web --parallel 3
        '
        package web build/web/_deps/raylib-src
        ;;
    *) echo "Unknown build target: $target" >&2; exit 2 ;;
esac
