#!/usr/bin/env bash
# Helpers shared by build-docker.sh and build-web-docker.sh (sourced, not run).

# Builds the Docker image when it does not exist yet.
bt3d_ensure_docker_image() {
    local image="$1"
    local dockerfile="$2"

    if ! docker image inspect "$image" >/dev/null 2>&1; then
        echo "=== docker build $image ==="
        docker build -t "$image" -f "$dockerfile" docker/
    fi
}

# Chooses the directory to mount as /work and sets MOUNT_SRC, STAGED and
# MIDI_MOUNT. Docker VMs such as colima only see $HOME, so a repo elsewhere
# (e.g. under /Volumes) is rsynced into the stage directory and built there.
bt3d_resolve_docker_mount() {
    local port_dir="$1"
    local stage_dir="$2"
    local midi_src="$3"

    if [[ "$port_dir" == "$HOME"* ]]; then
        MOUNT_SRC="$port_dir"
        STAGED=0
        MIDI_MOUNT="$midi_src"
        echo "=== docker mount: $MOUNT_SRC (direct) ==="
        return
    fi

    echo "=== repo outside \$HOME; rsync -> $stage_dir ==="
    mkdir -p "$stage_dir"
    rsync -a --delete \
        --exclude build \
        --exclude build-prof --exclude build-release --exclude build-switch \
        --exclude build-zig-windows --exclude build-zig-windows-ninja4 \
        --exclude dist --exclude .git --exclude docs \
        --exclude .DS_Store --exclude "*.swp" --exclude "*.swo" \
        "$port_dir/" "$stage_dir/"
    if [ -f "$midi_src" ]; then
        mkdir -p "$stage_dir/build-input"
        rsync -aL "$midi_src" "$stage_dir/build-input/m1.dat"
    fi
    MOUNT_SRC="$stage_dir"
    STAGED=1
    MIDI_MOUNT="$stage_dir/build-input/m1.dat"
}
