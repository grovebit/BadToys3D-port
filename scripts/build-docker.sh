#!/usr/bin/env bash
# Build Windows + Switch targets inside a single docker image (zig cross +
# devkitpro). macOS .app is not produced here.
#
# Usage: build-docker.sh [win|switch|all]  (default: all)
#
# Docker daemon must be running. If the repo is not reachable from the docker
# VM (e.g. colima's virtiofs only sees $HOME but the repo lives under
# /Volumes), the script stages the project into $DOCKER_STAGE (default
# $HOME/bt3d-switch/raylib-port) and builds from there, writing artifacts back.
set -euo pipefail

cd "$(dirname "$0")/.."
PORT_DIR="$(pwd)"

IMAGE="${BT3D_BUILD_IMAGE:-bt3d-build:latest}"
DOCKERFILE="docker/Dockerfile"
# Known-good staging path under $HOME. (Colima virtiofs sometimes doesn't
# pick up brand-new top-level dirs created in-session without a VM restart;
# reusing an existing path avoids that glitch.)
DOCKER_STAGE="${DOCKER_STAGE:-$HOME/bt3d-switch/raylib-port}"
DATA_PCK_SRC="${DATA_PCK_SRC:-$PORT_DIR/../data.pck}"
MIDI_SRC="${MIDI_SRC:-$PORT_DIR/../original/m1.dat}"
WIN_BUILD_DIR="${BT3D_WIN_BUILD_DIR:-build/windows}"
SWITCH_BUILD_DIR="${BT3D_SWITCH_BUILD_DIR:-build/switch}"
DOCKER_CLEAN="${BT3D_DOCKER_CLEAN:-0}"
DOCKER_RECONFIGURE="${BT3D_DOCKER_RECONFIGURE:-0}"

TARGETS="${1:-all}"
case "$TARGETS" in
    all|win|switch) ;;
    *) echo "usage: $0 [all|win|switch]" >&2; exit 2 ;;
esac

source "$PORT_DIR/scripts/docker-common.sh"
bt3d_ensure_docker_image "$IMAGE" "$DOCKERFILE"
bt3d_resolve_docker_mount "$PORT_DIR" "$DOCKER_STAGE" "$MIDI_SRC"

# Resolve data.pck path on host for the :ro bind mount
if [ "$STAGED" = 1 ]; then
    if [ -f "$DATA_PCK_SRC" ]; then
        [ -L "$DOCKER_STAGE/../data.pck" ] && rm -f "$DOCKER_STAGE/../data.pck"
        rsync -aL "$DATA_PCK_SRC" "$DOCKER_STAGE/../data.pck"
    fi
    DATA_MOUNT="$DOCKER_STAGE/../data.pck"
else
    DATA_MOUNT="$DATA_PCK_SRC"
fi

BUILD_WIN=0; BUILD_SW=0
case "$TARGETS" in
    all)    BUILD_WIN=1; BUILD_SW=1 ;;
    win)    BUILD_WIN=1 ;;
    switch) BUILD_SW=1 ;;
esac

DOCKER_CMD=(docker run --rm -v "$MOUNT_SRC:/work")
if [ -f "$DATA_MOUNT" ]; then
    DOCKER_CMD+=(-v "$DATA_MOUNT:/work/romfs/data.pck:ro")
fi
if [ -f "$MIDI_MOUNT" ]; then
    DOCKER_CMD+=(-v "$MIDI_MOUNT:/tmp/bt3d_m1.dat:ro" -e BT3D_MIDI_SRC=/tmp/bt3d_m1.dat)
fi
DOCKER_CMD+=(-e BUILD_WIN="$BUILD_WIN" -e BUILD_SW="$BUILD_SW" -e WIN_BUILD_DIR="$WIN_BUILD_DIR" -e SWITCH_BUILD_DIR="$SWITCH_BUILD_DIR" -e BT3D_DOCKER_CLEAN="$DOCKER_CLEAN" -e BT3D_DOCKER_RECONFIGURE="$DOCKER_RECONFIGURE" "$IMAGE" bash -c '
    set -e
    cd /work
    git config --global --add safe.directory "/work/$WIN_BUILD_DIR/_deps/raylib-src"

    if [ "$BUILD_WIN" = 1 ]; then
        echo "=== Windows: cmake + zig cross ==="
        if [ "${BT3D_DOCKER_CLEAN:-0}" = 1 ]; then
            rm -rf "$WIN_BUILD_DIR"
        fi
        if [ "${BT3D_DOCKER_RECONFIGURE:-0}" = 1 ] || [ ! -f "$WIN_BUILD_DIR/build.ninja" ]; then
            cmake -S . -B "$WIN_BUILD_DIR" -G Ninja \
                -DCMAKE_TOOLCHAIN_FILE=cmake/zig-windows-toolchain.cmake \
                -DBT3D_MIDI_SRC="${BT3D_MIDI_SRC:-/work/../original/m1.dat}" \
                -DBT3D_FETCH_RAYLIB=ON
        else
            echo "=== Windows: skip configure (cached build.ninja) ==="
        fi
        cmake --build "$WIN_BUILD_DIR"
        ls -lh "$WIN_BUILD_DIR/bt3d_raylib.exe"
    fi

    if [ "$BUILD_SW" = 1 ]; then
        echo "=== Switch: cmake + devkitpro ==="
        if [ "${BT3D_DOCKER_CLEAN:-0}" = 1 ]; then
            rm -rf "$SWITCH_BUILD_DIR"
        fi
        if [ "${BT3D_DOCKER_RECONFIGURE:-0}" = 1 ] || [ ! -f "$SWITCH_BUILD_DIR/Makefile" ]; then
            cmake -S . -B "$SWITCH_BUILD_DIR" \
                -DCMAKE_TOOLCHAIN_FILE=cmake/switch-toolchain.cmake \
                -DBT3D_MIDI_SRC="${BT3D_MIDI_SRC:-/work/../original/m1.dat}" \
                -DBT3D_PLATFORM_SWITCH=ON
        else
            echo "=== Switch: skip configure (cached Makefile) ==="
        fi
        cmake --build "$SWITCH_BUILD_DIR" --parallel "$(nproc)"
        ls -lh "$SWITCH_BUILD_DIR/bt3d_raylib_nx.nro"
    fi
')

"${DOCKER_CMD[@]}"

# Copy artifacts back if we built from a staged copy
if [ "$STAGED" = 1 ]; then
    if [ "$BUILD_WIN" = 1 ] && [ -f "$DOCKER_STAGE/$WIN_BUILD_DIR/bt3d_raylib.exe" ]; then
        mkdir -p "$PORT_DIR/$WIN_BUILD_DIR"
        cp "$DOCKER_STAGE/$WIN_BUILD_DIR/bt3d_raylib.exe" \
           "$PORT_DIR/$WIN_BUILD_DIR/bt3d_raylib.exe"
    fi
    if [ "$BUILD_SW" = 1 ] && [ -f "$DOCKER_STAGE/$SWITCH_BUILD_DIR/bt3d_raylib_nx.nro" ]; then
        mkdir -p "$PORT_DIR/$SWITCH_BUILD_DIR"
        cp "$DOCKER_STAGE/$SWITCH_BUILD_DIR/bt3d_raylib_nx.nro" \
           "$PORT_DIR/$SWITCH_BUILD_DIR/bt3d_raylib_nx.nro"
    fi
fi

echo "=== done ==="
