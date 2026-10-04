# Build and play

You need the original game's `data.pck`. It contains the levels, graphics and
sounds and is not included in this repository.

## 1. Get the project

Download and extract the repository ZIP, or clone it with Git. Open Terminal
in the extracted project folder (the folder containing `build.sh`). Keep the
folder somewhere writable, such as your home directory.

## 2. Install the tools once

### macOS

Install Apple's command-line tools, then wait for the installer to finish:

```bash
xcode-select --install
```

Install CMake using [Homebrew](https://brew.sh/) if you have it:

```bash
brew install cmake
```

Alternatively, install [CMake](https://cmake.org/download/) and follow its
instructions for adding the command-line tools to `PATH`. Check that
`cmake --version` works in a new Terminal window. Git, the compiler and Make
come with Apple's command-line tools; you do not need to install raylib.

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install build-essential cmake git libasound2-dev libx11-dev \
  libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev \
  libxcursor-dev libxinerama-dev
```

For other Linux distributions, install CMake, Git, C/C++ compilers, Make and the
[raylib system dependencies](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux).
The native build uses X11; Wayland desktops need XWayland available to run it.

### Windows

Use the [Windows steps below](#windows-build) to select `--win`.

## 3. Add the game data and play

Copy `data.pck` from your original Bad Toys 3D installation into the project
folder, next to `build.sh`. Then run:

```bash
./build.sh --run
```

The script checks for tools, downloads and builds raylib 5.5, builds the game,
copies the pack into place and starts it. The first build needs internet access
and takes longer; later runs reuse the build. It does not install system packages.

You can also point at the pack without copying it first:

```bash
./build.sh --data "/path/with spaces/data.pck" --run
```

The script also checks `../data.pck`, `../original/data.pck`, `romfs/data.pck`
in that order after the project folder. Native builds also reuse the previous
copy in `build/play/`; macOS app builds can reuse `dist/macos/data.pck`.
An explicit `--data` path takes priority over `DATA_PCK_SRC` and auto-detection.

Run `./build.sh --run` again to rebuild and play, or launch `build/play/bt3d_raylib`
directly. Saves and settings stay in `build/play/`. Use `./build.sh`
to build without launching. A build can proceed without data, but `--run`
requires it. `--run` works with the default native target or `--macos` only. Optional menu music uses `../original/m1.dat`;
set `MIDI_SRC=/absolute/path/to/m1.dat` to use another location.

## Windows build

Use Docker Desktop with an Ubuntu WSL terminal:

1. Install [WSL with Ubuntu](https://learn.microsoft.com/en-us/windows/wsl/install)
   and [Docker Desktop](https://www.docker.com/products/docker-desktop/).
2. Start Docker Desktop and enable Ubuntu under **Settings → Resources →
   WSL Integration**. See [Docker's WSL setup](https://docs.docker.com/desktop/features/wsl/).
3. Download or clone this project into your Ubuntu home directory, then open
   that folder in the Ubuntu terminal. Run:

```bash
./build.sh --win
```

Put `data.pck` beside `build.sh` before building (or pass `--data`); the script
copies it into `build/windows/`. After the build completes, run
`explorer.exe build/windows` from Ubuntu to open that folder in Windows, then
double-click `bt3d_raylib.exe`. No local C compiler or raylib installation is
needed. See the [Docker guide](DOCKER_BUILD.md) for staging and custom paths.

## If setup stops

- **Missing tool:** finish the installation steps for your system, open a new
  terminal and retry.
- **Game data not found:** use `--data` with the full path to the original pack.
  Keep quotes around paths containing spaces.
- **Download failed:** check internet access, then rerun the same command.
- **Linux configure error mentioning X11 or OpenGL:** install the development
  packages above. The compiler error immediately above the setup message gives
  the missing dependency.
- **No window on Linux:** run from a graphical desktop session, not a headless
  SSH session; omit `--run` for build-only use.

For macOS app bundles, Switch or browser builds, see the
[distribution guide](DIST_BUILD.md). For manual CMake builds,
use the [developer commands](DEVELOPMENT.md#manual-build).

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
