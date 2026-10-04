# Windows builds with Zig

The standard path uses Docker and produces `build/windows/bt3d_raylib.exe`:

```bash
./build.sh --win
```

See the [Docker guide](DOCKER_BUILD.md) for prerequisites and configuration.
Put the original `data.pck` beside the executable to play. Missing data is
shown in-game.

## Direct cross-compilation

The repository also includes a CMake toolchain for building 64-bit Windows
executables with a local Zig installation. You need CMake, Zig, Ninja, Git,
and a Windows resource compiler (`llvm-windres` or
`x86_64-w64-mingw32-windres`) on `PATH`. Docker pins Zig 0.13.0.

```bash
cmake -S . -B build/windows-local -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/zig-windows-toolchain.cmake \
  -DBT3D_FETCH_RAYLIB=ON
cmake --build build/windows-local
```

Output: `build/windows-local/bt3d_raylib.exe`. Configure downloads raylib 5.5.
Use a separate directory from Docker builds to avoid cached host paths.

The toolchain searches for LLVM archiver tools (including Homebrew locations)
and falls back to the repository's `zig-ar`/`zig-ranlib` wrappers. If the default
Zig cache is not writable, set `ZIG_GLOBAL_CACHE_DIR` and `ZIG_LOCAL_CACHE_DIR`
to a writable directory for both configure and build.
