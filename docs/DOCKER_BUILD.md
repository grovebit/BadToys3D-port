# Docker builds

Windows and Switch share `docker/Dockerfile` (Zig/devkitPro/raylib-nx).
Web uses `docker/Dockerfile.web` (Emscripten). macOS builds locally.

## Requirements and commands

Use Bash and a running Docker daemon. On Windows, use a Bash environment
with access to Docker, such as WSL. Run from the repository root:

```bash
./build.sh --win
./build.sh --switch
./build.sh --web
./build.sh --win --switch --web
```

Images are built automatically when missing and reused thereafter. Initial
builds need network access. Supply the original assets with absolute host paths:

```bash
DATA_PCK_SRC=/path/to/data.pck MIDI_SRC=/path/to/m1.dat ./build.sh --switch
```

`build.sh` auto-detects `data.pck` in the project folder, `../data.pck`,
`../original/data.pck` or `romfs/data.pck`. Use `--data /path/to/data.pck` or
`DATA_PCK_SRC` to override. If present, it is mounted into the Switch
romfs and embedded in the NRO; web copies it to `dist/web/`. `build.sh --win` also copies it beside the Windows executable.
`MIDI_SRC` defaults to `../original/m1.dat`; missing MIDI disables menu music.

## Outputs and wrapper options

| Target | Output |
| --- | --- |
| Windows | `build/windows/bt3d_raylib.exe` |
| Switch | `build/switch/bt3d_raylib_nx.nro` |
| Web | `dist/web/index.html`, `bt3d_raylib_web.js`, `bt3d_raylib_web.wasm` |

- `--all`: macOS + Windows + Switch + Web; requires a macOS host.
- `--release`: builds all four and [stages distribution files](DIST_BUILD.md).
- `--stage-release`: stages existing artifacts without building.
- `--docker-image`: ensures the Windows/Switch image exists, then exits.
  It does not build the web image or refresh an existing image.
- `--clean`: removes `build/macos-release`, `dist/macos`, `build/switch`,
  `build/windows`, `build/release` and `build/web`, then exits. It leaves other
  staged distribution files, custom build directories and `build/play/` (which
  contains native saves) in place.
- `--docker`: accepted as a compatibility no-op.

## Configuration

| Environment variable | Default / purpose |
| --- | --- |
| `BT3D_BUILD_IMAGE` | `bt3d-build:latest` — Windows/Switch image |
| `BT3D_WEB_BUILD_IMAGE` | `bt3d-web-build:latest` — web image |
| `BT3D_DOCKER_CLEAN=1` | Removes the selected Docker target's build directory before building |
| `BT3D_DOCKER_RECONFIGURE=1` | Forces configure instead of reusing cached build files |
| `BT3D_WIN_BUILD_DIR` | `build/windows` |
| `BT3D_SWITCH_BUILD_DIR` | `build/switch` |
| `BT3D_WEB_BUILD_DIR` | `build/web` |
| `BT3D_WEB_DIST_DIR` | `dist/web` |
| `DOCKER_STAGE` | Staging directory for repositories outside `$HOME` (below) |

Use reconfigure after changing toolchain/CMake options or MIDI source paths.
Build-directory overrides are relative to the repository mount; release staging
still reads the default Windows/Switch/Web build paths.

To rebuild images after editing a Dockerfile:

```bash
docker build -t bt3d-build:latest -f docker/Dockerfile docker/
docker build -t bt3d-web-build:latest -f docker/Dockerfile.web docker/
```

## Repositories outside the home directory

The scripts mount repositories under `$HOME` directly. Otherwise they use
`rsync` to stage a copy under `~/bt3d-switch/raylib-port` (Windows/Switch) or
`~/bt3d-switch/raylib-port-web` (Web), then copy the artifacts back.

Set `DOCKER_STAGE` to override this path. Use a dedicated directory: staging
uses `rsync --delete`. It excludes build outputs, `dist/`, `.git`, `docs/` and
editor temporary files. The host needs `rsync` for this path.

## Lower-level and legacy commands

```bash
./scripts/build-docker.sh all     # Windows + Switch only
./scripts/build-web-docker.sh     # Web only
./scripts/build-all.sh win,switch # compatibility wrapper
```

Direct lower-level scripts retain `../data.pck` as their default; automatic
discovery and `--data` are provided by `build.sh`.

The legacy `build-all.sh all` delegates to `build.sh --all`, including macOS
and Web. Prefer explicit targets when building on Linux or Windows.
