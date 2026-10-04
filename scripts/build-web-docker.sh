#!/usr/bin/env bash
# Build the browser/WASM target inside an Emscripten Docker image.
set -euo pipefail

cd "$(dirname "$0")/.."
PORT_DIR="$(pwd)"

IMAGE="${BT3D_WEB_BUILD_IMAGE:-bt3d-web-build:latest}"
DOCKERFILE="docker/Dockerfile.web"
DOCKER_STAGE="${DOCKER_STAGE:-$HOME/bt3d-switch/raylib-port-web}"
MIDI_SRC="${MIDI_SRC:-$PORT_DIR/../original/m1.dat}"
DATA_PCK_SRC="${DATA_PCK_SRC:-$PORT_DIR/../data.pck}"
WEB_BUILD_DIR="${BT3D_WEB_BUILD_DIR:-build/web}"
OUT_DIR="${BT3D_WEB_DIST_DIR:-dist/web}"
DOCKER_CLEAN="${BT3D_DOCKER_CLEAN:-0}"
DOCKER_RECONFIGURE="${BT3D_DOCKER_RECONFIGURE:-0}"

source "$PORT_DIR/scripts/docker-common.sh"
bt3d_ensure_docker_image "$IMAGE" "$DOCKERFILE"
bt3d_resolve_docker_mount "$PORT_DIR" "$DOCKER_STAGE" "$MIDI_SRC"

DOCKER_CMD=(docker run --rm -v "$MOUNT_SRC:/work")
if [ -f "$MIDI_MOUNT" ]; then
    DOCKER_CMD+=(-v "$MIDI_MOUNT:/tmp/bt3d_m1.dat:ro" -e BT3D_MIDI_SRC=/tmp/bt3d_m1.dat)
fi
DOCKER_CMD+=(-e WEB_BUILD_DIR="$WEB_BUILD_DIR" -e OUT_DIR="$OUT_DIR" -e BT3D_DOCKER_CLEAN="$DOCKER_CLEAN" -e BT3D_DOCKER_RECONFIGURE="$DOCKER_RECONFIGURE" "$IMAGE" bash -c '
    set -e
    cd /work
    git config --global --add safe.directory "/work/$WEB_BUILD_DIR/_deps/raylib-src"

    if [ "${BT3D_DOCKER_CLEAN:-0}" = 1 ]; then
        rm -rf "$WEB_BUILD_DIR"
    fi
    if [ "${BT3D_DOCKER_RECONFIGURE:-0}" = 1 ] || [ ! -f "$WEB_BUILD_DIR/build.ninja" ]; then
        emcmake cmake -S . -B "$WEB_BUILD_DIR" -G Ninja \
            -DBT3D_PLATFORM_WEB=ON \
            -DBT3D_MIDI_SRC="${BT3D_MIDI_SRC:-/work/../original/m1.dat}"
    else
        echo "=== Web: skip configure (cached build.ninja) ==="
    fi

    rm -f "$WEB_BUILD_DIR"/bt3d_raylib_web.data
    cmake --build "$WEB_BUILD_DIR"
    rm -rf "$OUT_DIR"
    mkdir -p "$OUT_DIR"
    cp "$WEB_BUILD_DIR"/bt3d_raylib_web.html "$OUT_DIR/index.html"
    cp "$WEB_BUILD_DIR"/bt3d_raylib_web.js "$OUT_DIR/"
    cp "$WEB_BUILD_DIR"/bt3d_raylib_web.wasm "$OUT_DIR/"
    find "$OUT_DIR" -type f -print
')

"${DOCKER_CMD[@]}"

if [ "$STAGED" = 1 ]; then
    rm -rf "$PORT_DIR/$OUT_DIR"
    mkdir -p "$PORT_DIR/$OUT_DIR"
    cp -a "$DOCKER_STAGE/$OUT_DIR/." "$PORT_DIR/$OUT_DIR/"
fi

if [ -f "$DATA_PCK_SRC" ]; then
    mkdir -p "$PORT_DIR/$OUT_DIR"
    cp -aL "$DATA_PCK_SRC" "$PORT_DIR/$OUT_DIR/data.pck"
fi

echo "=== done ==="
