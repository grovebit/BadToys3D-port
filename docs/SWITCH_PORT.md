# Nintendo Switch Build Notes

This project targets Nintendo Switch through `raylib-nx`.

Reference repository:

- https://github.com/Kapilarny/raylib-nx

## Build

From the repository root, with Bash and Docker available:

```bash
DATA_PCK_SRC=/path/to/data.pck ./build.sh --switch
```

Output: `build/switch/bt3d_raylib_nx.nro`. The build embeds the available pack
in romfs and includes `icon.jpg` and NACP metadata. See the
[Docker guide](DOCKER_BUILD.md) for configuration and staging.

For a local devkitPro build, install the Switch toolchain and Mesa/libdrm
libraries, then build raylib-nx with the compatibility patches in
[`docker/Dockerfile`](../docker/Dockerfile). That file is the maintained
reference for dependency setup; an unpatched upstream build may fail.
With headers and `libraylib.a` installed under `$DEVKITPRO/portlibs/switch`:

```bash
cp /path/to/data.pck romfs/data.pck
cmake -S . -B build/switch-local \
  -DCMAKE_TOOLCHAIN_FILE=cmake/switch-toolchain.cmake \
  -DBT3D_PLATFORM_SWITCH=ON
cmake --build build/switch-local --parallel
```

This produces the ELF and `.nro` in `build/switch-local/`. Keep local and Docker
build directories separate so their cached toolchain paths do not conflict.

## Running

Copy the `.nro` to the Switch SD card:

```
sdmc:/switch/bt3d/bt3d_raylib_nx.nro
```

Launch via Homebrew Menu (Album or title override).

If not embedding assets in romfs, also copy `data.pck`:

```
sdmc:/switch/bt3d/data.pck
```

Saves are written to `sdmc:/switch/bt3d/`.

## Controls and implementation

See the [Switch controls](SETUP.md#nintendo-switch) for the current bindings.

The shared runtime lives in `src/runtime.c`. Switch-specific input uses libnx
in `src/input.c`; asset/save paths and window setup live in `src/platform.c`.
`cmake/switch-toolchain.cmake` configures the compiler, and `cmake/switch.cmake`
handles linking and NRO packaging.

Save names are generated automatically on Switch; there is no text-entry UI.
