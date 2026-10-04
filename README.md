# Bad Toys 3D — raylib Port

C11/raylib port of Bad Toys 3D (Tibo Software, 1995–1998) for macOS,
Linux, Windows, Nintendo Switch and desktop browsers. Android is an
[unfinished scaffold](docs/ANDROID_PORT_PLAN.md).

## Quick start

On **macOS or Linux**, install the tools in the [setup guide](docs/SETUP.md),
copy the original game's `data.pck` next to `build.sh`, then run:

```bash
./build.sh --run
```

This downloads raylib, builds the game, puts the data in place and starts it.
Later runs reuse the build. You can also use
`./build.sh --data "/path/to/data.pck" --run`. Omit `--run` to build only.

On **Windows**, follow the [Docker setup steps](docs/SETUP.md#windows-build).
The original game data is required on every platform and is not included here.

## Platform builds

```bash
./build.sh --macos                # local macOS build and .app bundle
./build.sh --win                  # Windows via Docker/Zig
./build.sh --switch               # Switch via Docker/devkitPro
./build.sh --web                  # browser via Docker/Emscripten
./build.sh --win --switch --web   # all Docker targets, on any supported host
./build.sh --release              # all four targets and dist/ staging; requires macOS
```

`--native` (the default) and `--macos` fetch raylib automatically. They require
CMake, Git and the host compiler tools; see [setup](docs/SETUP.md). `--all` builds the same four targets as `--release`, without
its final staging pass. Docker targets require Bash and a running Docker daemon.

| Target | Default output | Guide |
| --- | --- | --- |
| macOS app | `dist/macos/Bad Toys 3D.app` | [Distribution](docs/DIST_BUILD.md) |
| Windows | `build/windows/bt3d_raylib.exe` | [Docker](docs/DOCKER_BUILD.md) |
| Switch | `build/switch/bt3d_raylib_nx.nro` | [Switch](docs/SWITCH_PORT.md) |
| Browser | `dist/web/index.html` and JS/WASM files | [Web](docs/WEB_BUILD.md) |

`build.sh` finds `data.pck` in the project folder, `../data.pck`,
`../original/data.pck` or `romfs/data.pck`. Use `--data /path/to/data.pck`
(or `DATA_PCK_SRC`) to select a file explicitly. Available data is copied beside
native/Windows executables, packaged for Switch, and staged for Web.

## Runtime files

- **Desktop:** put `data.pck` beside the executable, or beside the macOS
  `.app` bundle. Saves and `config.dat` are written in that same directory,
  which must be writable.
- **Switch:** the Docker build embeds the available pack in the `.nro`.
  Without an embedded pack, place it in `sdmc:/switch/bt3d/`; saves also go there.
- **Browser:** serve `dist/web/` over HTTP. The page loads a neighbouring
  `data.pck`, or lets you choose/drop it, then enables **Start**. Saves and
  config persist through IndexedDB.

```bash
cd dist/web
python3 -m http.server 8000
```

Open `http://127.0.0.1:8000/`; `file://` does not work.

## Controls

### Desktop
- `WASD` or `↑` / `↓` — move forward/back; `A` / `D` strafe
- `←` / `→` — turn (yaw)
- Mouse — look
- Left click or `Left Ctrl` — shoot
- `E` / `Space` / right click — use/open
- `1`–`4` — select weapon
- `M` — map
- `Esc` — menu
- `F5` — save, `F9` — load
- `Backspace` — delete the selected save (load menu)
- Gamepad: the Switch buttons below, by position (noclip and profiler are keyboard-only)
- Debug tools only: `[` / `]` cycle maps, `N` noclip, `F10` profiler, `C` copy debug text

### Nintendo Switch
- Left stick / D-pad — move, and navigate menus
- Right stick — look
- ZR — fire
- ZL / A — use / interact
- L / R — cycle weapon
- X — map
- + — menu
- A — confirm, B — back, Y — delete the selected save (menus)
- Debug tools only: D-pad left/right cycle maps, Y noclip, − profiler

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

## Documentation

- [First-time setup](docs/SETUP.md) — install tools, add game data and play
- [Distribution builds](docs/DIST_BUILD.md) — packaging and staging
- [Docker builds](docs/DOCKER_BUILD.md) — images, options and troubleshooting
- [Web build](docs/WEB_BUILD.md), [Switch build](docs/SWITCH_PORT.md), [direct Windows build](docs/WINDOWS_ZIG_BUILD.md)
- [Code specification](docs/CODE_SPEC.md) — modules, runtime flow and save format
- [Difficulty tables](docs/DIFFICULTY_TABLES.md) — enemy stats and weapon damage
- [Release notes](docs/RELEASE_NOTES.md) — current behavior and limitations
- [Changelog](docs/CHANGELOG.md) and [porting history](docs/PORTING_NOTES.md) — historical changes and decisions
- [Android plan](docs/ANDROID_PORT_PLAN.md) and [cleanup backlog](docs/CLEANUP_PLAN.md)
