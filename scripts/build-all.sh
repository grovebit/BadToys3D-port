#!/usr/bin/env bash
# Compatibility wrapper for the old build-all.sh interface.
set -euo pipefail

cd "$(dirname "$0")/.."

target_arg="${1:-all}"
build_args=()

if [ "$target_arg" = "all" ]; then
    build_args+=(--all)
else
    IFS=, read -r -a targets <<< "$target_arg"
    for target in "${targets[@]}"; do
        case "$target" in
            win) build_args+=(--win) ;;
            switch) build_args+=(--switch) ;;
            *) echo "usage: $0 [all|win|switch|win,switch]" >&2; exit 2 ;;
        esac
    done
fi

exec ./build.sh "${build_args[@]}"
