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

## Automated releases

GitHub Actions builds macOS (Intel and Apple Silicon), Windows, Linux, Switch,
and web whenever you push to `main`. After all builds and native tests pass,
it updates the **Development** prerelease with ZIPs, source details and checksums.
Game data and the separate menu MIDI asset are never bundled.

To publish a numbered release from the current commit:

```bash
git tag v0.1.1
git push grovebit v0.1.1
```

Use a new version each time. Tags with a suffix, such as `v0.2.0-rc.1`, create
prereleases. Published numbered releases are not overwritten.

Follow progress under **Actions → Build and release** on GitHub. You can rerun
failed jobs there or use **Run workflow** on `main`. Builds run on GitHub's
machines; your computer does not need to stay on. Private repositories use your
account's Actions allowance. The workflow uses GitHub's built-in token; no
personal access token or game assets need to be added as secrets.
