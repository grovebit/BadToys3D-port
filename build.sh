#!/usr/bin/env bash
# Build wrapper for the Bad Toys 3D raylib port.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
data_src="${DATA_PCK_SRC:-}"
run_game=false
target_native=false

IMAGE="${BT3D_BUILD_IMAGE:-bt3d-build:latest}"
DOCKERFILE="docker/Dockerfile"

want_all=false
want_release=false
want_stage_release=false
want_clean=false
want_docker_image=false
target_win=false
target_switch=false
target_web=false
target_macos=false

usage() {
    cat <<EOF
Usage: $0 [--<target>] [--data /path/to/data.pck] [--run]

With no target, builds locally on macOS/Linux into build/play/.

Targets:
  --native      Build the local macOS/Linux binary (default)
  --macos      Build the local macOS binary and stage the .app bundle
  --win         Build Windows .exe in Docker with Zig
  --switch      Build Nintendo Switch .nro in Docker with devkitPro
  --web         Build browser/WASM bundle in Docker with Emscripten
  --all         Build macOS + Windows + Switch + Web

Options:
  --data PATH    Use this data.pck (otherwise auto-detect)
  --run          Launch after a --native or --macos build
  --docker-image  Build the Docker image only
  --release       Build all targets and stage artifacts in dist/
  --stage-release Stage existing build artifacts in dist/ without rebuilding
  --clean         Remove generated build outputs

Examples:
  $0
  $0 --run
  $0 --data "/path/to/data.pck" --run
  $0 --macos
  $0 --win
  $0 --switch
  $0 --web
  $0 --all
  $0 --release
  $0 --stage-release
EOF
}

log() {
    printf '\n==> %s\n' "$*"
}

build_docker_image() {
    if docker image inspect "$IMAGE" >/dev/null 2>&1; then
        log "Docker image already exists: $IMAGE"
        return
    fi

    log "Building Docker image: $IMAGE"
    docker build -t "$IMAGE" -f "$DOCKERFILE" docker/
}

build_docker_targets() {
    local docker_target="$1"

    log "Building $docker_target in Docker"
    bash scripts/build-docker.sh "$docker_target"
}

build_web_target() {
    log "Building web in Docker"
    bash scripts/build-web-docker.sh
}

fail() {
    printf '\n%s\nSee %s/docs/SETUP.md for setup instructions.\n' "$1" "$ROOT" >&2
    exit 1
}

check_native_tools() {
    case "$(uname -s)" in
        Darwin)
            xcode-select -p >/dev/null 2>&1 || fail "Install Apple command-line tools with: xcode-select --install"
            ;;
        Linux) ;;
        *) fail "Native builds support macOS and Linux. For Windows, use --win with Docker (see docs/SETUP.md)." ;;
    esac
    for tool in cmake git cc c++ make; do
        command -v "$tool" >/dev/null 2>&1 || fail "Missing prerequisite: $tool. Install the tools listed for your system, then retry."
    done
}

resolve_data_pack() {
    if [ -n "$data_src" ]; then
        [ -f "$data_src" ] && [ -r "$data_src" ] || fail "Cannot read data pack: $data_src"
        data_src="$(cd "$(dirname "$data_src")" && pwd)/$(basename "$data_src")"
    else
        for candidate in "$ROOT/data.pck" "$ROOT/../data.pck" "$ROOT/../original/data.pck" "$ROOT/romfs/data.pck"; do
            if [ -f "$candidate" ] && [ -r "$candidate" ]; then
                data_src="$candidate"
                break
            fi
        done
        if [ -z "$data_src" ] && [ "$target_native" = true ] && [ -r "$ROOT/build/play/data.pck" ]; then
            data_src="$ROOT/build/play/data.pck"
        fi
        if [ -z "$data_src" ] && [ "$target_macos" = true ] && [ -r "$ROOT/dist/macos/data.pck" ]; then
            data_src="$ROOT/dist/macos/data.pck"
        fi
    fi
    export DATA_PCK_SRC="$data_src"
}

build_native_binary() {
    local build_dir="$1"
    local cmake_args=(-S . -B "$build_dir" -DBT3D_FETCH_RAYLIB=ON -DCMAKE_BUILD_TYPE=Release)
    if [ -n "${MIDI_SRC:-}" ]; then
        cmake_args+=("-DBT3D_MIDI_SRC=$MIDI_SRC")
    fi
    log "Building native binary (the first run downloads raylib 5.5)"
    cmake "${cmake_args[@]}" || fail "Configure failed. Check the error above and your prerequisites. The first build needs internet access."
    cmake --build "$build_dir" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}" || fail "Build failed. Check the compiler error above."
}

build_macos_target() {
    local build_dir="${BT3D_MACOS_BUILD_DIR:-build/macos-release}"
    build_native_binary "$build_dir"
    log "Packaging macOS .app"
    BUILD_DIR="$build_dir" bash scripts/package-mac-app.sh
}

clean_outputs() {
    log "Removing generated build outputs"
    rm -rf \
        build/macos-release \
        dist/macos \
        build/switch \
        build/windows \
        build/release \
        build/web
}

copy_if_exists() {
    local src="$1"
    local dst="$2"

    if [ -e "$src" ]; then
        mkdir -p "$(dirname "$dst")"
        cp -a "$src" "$dst"
    fi
}

copy_data_pck_if_exists() {
    local dst="$1"
    local src="${DATA_PCK_SRC:-../data.pck}"

    if [ -f "$src" ]; then
        mkdir -p "$(dirname "$dst")"
        if [ ! -e "$dst" ] || ! cmp -s "$src" "$dst"; then
            cp -aL "$src" "$dst"
        fi
    fi
}

stage_release() {
    local outdir="dist"

    log "Staging release artifacts in $outdir"
    mkdir -p "$outdir/windows" "$outdir/switch" "$outdir/web"
    rm -f "$outdir/macos.zip" "$outdir/windows.zip" "$outdir/switch.zip"

    copy_if_exists "build/windows/bt3d_raylib.exe" "$outdir/windows/bt3d_raylib.exe"
    copy_if_exists "build/switch/bt3d_raylib_nx.nro" "$outdir/switch/bt3d_raylib_nx.nro"
    if [ -f "build/web/bt3d_raylib_web.html" ]; then
        cp -a build/web/bt3d_raylib_web.html "$outdir/web/index.html"
        cp -a build/web/bt3d_raylib_web.js "$outdir/web/"
        cp -a build/web/bt3d_raylib_web.wasm "$outdir/web/"
        copy_data_pck_if_exists "$outdir/web/data.pck"
    fi
    copy_data_pck_if_exists "$outdir/windows/data.pck"
    copy_data_pck_if_exists "$outdir/switch/data.pck"
    copy_data_pck_if_exists "$outdir/shared/data.pck"

    if [ -x "${BT3D_MACOS_BUILD_DIR:-build/macos-release}/bt3d_raylib" ] || [ -x build-release/bt3d_raylib ] || [ -x build/bt3d_raylib ]; then
        BUILD_DIR="${BT3D_MACOS_BUILD_DIR:-build/macos-release}" bash scripts/package-mac-app.sh
    fi

    find "$outdir" -type f -print
}

print_artifacts() {
    log "Artifacts"
    [ "$target_native" = true ] && \
        ls -lh "build/play/bt3d_raylib" 2>/dev/null || true
    [ "$target_macos" = true ] && \
        ls -ld "dist/macos/Bad Toys 3D.app" 2>/dev/null || true
    [ "$target_win" = true ] && \
        ls -lh "build/windows/bt3d_raylib.exe" 2>/dev/null || true
    [ "$target_switch" = true ] && \
        ls -lh "build/switch/bt3d_raylib_nx.nro" 2>/dev/null || true
    [ "$target_web" = true ] && \
        ls -lh "dist/web/index.html" "dist/web/bt3d_raylib_web.js" "dist/web/bt3d_raylib_web.wasm" 2>/dev/null || true
}

while [ $# -gt 0 ]; do
    case "$1" in
        --native) target_native=true ;;
        --run) run_game=true ;;
        --data)
            [ $# -ge 2 ] && [ -n "$2" ] || fail "--data needs the path to data.pck."
            data_src="$2"
            shift
            ;;
        --all) want_all=true ;;
        --macos|--mac|--osx) target_macos=true ;;
        --win) target_win=true ;;
        --switch|--nx) target_switch=true ;;
        --web) target_web=true ;;
        --docker) ;;
        --docker-image) want_docker_image=true ;;
        --release) want_release=true; want_all=true ;;
        --stage-release) want_stage_release=true ;;
        --clean) want_clean=true ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [ "$want_all" = true ]; then
    target_macos=true
    target_win=true
    target_switch=true
    target_web=true
fi

if [ "$run_game" = true ]; then
    if [ "$want_clean" = true ] || [ "$want_stage_release" = true ] || [ "$want_docker_image" = true ] ||
       [ "$target_win" = true ] || [ "$target_switch" = true ] || [ "$target_web" = true ] ||
       { [ "$target_native" = true ] && [ "$target_macos" = true ]; }; then
        fail "--run requires a single native target (--native or --macos), without maintenance options."
    fi
fi

# Maintenance commands do not need native build tools or game data.
if [ "$want_clean" = true ]; then
    cd "$ROOT"
    clean_outputs
    exit 0
fi
if [ "$want_docker_image" = true ]; then
    cd "$ROOT"
    build_docker_image
    exit 0
fi

if [ "$want_stage_release" = false ] && [ "$target_native" = false ] && [ "$target_macos" = false ] && [ "$target_win" = false ] && [ "$target_switch" = false ] && [ "$target_web" = false ]; then
    target_native=true
fi

# Resolve user paths relative to the caller before entering the repository.
resolve_data_pack
cd "$ROOT"

if [ "$want_stage_release" = true ]; then
    stage_release
    exit 0
fi

if [ "$target_macos" = true ] && [ "$(uname -s)" != Darwin ]; then
    fail "macOS .app builds require macOS. Use --native locally, or --win --switch --web for Docker targets."
fi
if [ "$target_native" = true ] || [ "$target_macos" = true ]; then
    check_native_tools
fi
if [ "$run_game" = true ] && [ -z "$data_src" ]; then
    fail "Game data not found. Put data.pck beside build.sh, or use --data /path/to/data.pck."
fi
if [ -z "$data_src" ]; then
    log "No data.pck found; building without game data. Add it later or use --data /path/to/data.pck."
fi
if [ "$target_win" = true ] || [ "$target_switch" = true ] || [ "$target_web" = true ]; then
    command -v docker >/dev/null 2>&1 || fail "Docker is missing. Install and start Docker, then retry."
    docker info >/dev/null 2>&1 || fail "Docker is not ready. Start Docker and check that docker info works."
fi

if [ "$target_native" = true ]; then
    build_native_binary build/play
    copy_data_pck_if_exists build/play/data.pck
fi

if [ "$target_macos" = true ]; then
    build_macos_target
fi

if [ "$target_win" = true ] || [ "$target_switch" = true ]; then
    docker_target=all
    if [ "$target_win" = true ] && [ "$target_switch" = false ]; then
        docker_target=win
    elif [ "$target_win" = false ] && [ "$target_switch" = true ]; then
        docker_target=switch
    fi
    build_docker_targets "$docker_target"
    if [ "$target_win" = true ]; then
        copy_data_pck_if_exists "${BT3D_WIN_BUILD_DIR:-build/windows}/data.pck"
    fi
fi

if [ "$target_web" = true ]; then
    build_web_target
fi

if [ "$want_release" = true ]; then
    stage_release
fi

print_artifacts

if [ "$run_game" = true ]; then
    if [ "$target_macos" = true ]; then
        exec "dist/macos/Bad Toys 3D.app/Contents/MacOS/bt3d_raylib"
    fi
    exec ./build/play/bt3d_raylib
fi
