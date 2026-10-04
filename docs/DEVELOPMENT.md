# Development

## Manual build

For an existing development environment (CMake 3.16+, C compiler and raylib):

```bash
cmake -S . -B build/desktop
cmake --build build/desktop --parallel
cp /path/to/data.pck build/desktop/data.pck
./build/desktop/bt3d_raylib
```

Add `-DBT3D_FETCH_RAYLIB=ON` to configure to download raylib 5.5 instead of
using an installed copy. Builds default to Release; use
`-DCMAKE_BUILD_TYPE=Debug` for debugging. Optional menu music is embedded from
`../original/m1.dat`; override with `-DBT3D_MIDI_SRC=/path/to/m1.dat`.

## Tests

```bash
cmake -S . -B build/tests -DBT3D_BUILD_TESTS=ON
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure
```

Build script checks (Python 3, no compiler or game data required):

```bash
python3 tests/test_build.py
```

The C tests use the same raylib prerequisites as a native build. Tests cover pack/map
parsers; native macOS/Linux builds also include headless gameplay tests for
save/load, failed writes and loads, cooldowns, death/restart and level transitions.
