# Bad Toys 3D — raylib Port

A C11/raylib port of **Bad Toys 3D**, the first-person shooter by Tibo Software
(1995–1998), for macOS, Linux, Windows, Nintendo Switch and web browsers.

## Why I made this

I played Bad Toys 3D as a child and have fond memories of it. The original
was built for older 32-bit Windows systems and doesn't run natively on many
modern devices. I started this port to revisit a childhood favourite and make
it playable on a wider range of today's hardware.

I chose [raylib](https://www.raylib.com/) for its lightweight design, simplicity
and broad platform support. It provides a good foundation for bringing the
game to different devices, though each platform still needs its own porting
work and testing.

## Bring your own assets

**Game assets are not included. You must provide `data.pck` from your own
original Bad Toys 3D installation.** It contains the levels, graphics and sounds.

The demo's `data.pck` can also be used. This port loads the campaign levels
present in the supplied pack without enforcing the original demo's level limit.
**Please use this port only with a valid license for the original Bad Toys 3D.**
Using demo assets does not grant a license to the full game.

## Download and play — no compiling needed

1. Open [Releases](https://github.com/grovebit/BadToys3D-port/releases) and download
   the ZIP for your platform under **Assets**. Choose a platform ZIP, not the
   **Source code** downloads.
2. Extract the ZIP into a writable folder, such as a folder in your home directory.
3. Copy your `data.pck` into the same folder as the game, then launch it:

| Platform | Where to put `data.pck` | How to launch |
| --- | --- | --- |
| Windows | Beside `bt3d_raylib.exe` | Double-click the EXE. |
| macOS | Beside `Bad Toys 3D.app`, **not inside it** | Open the app. |
| Linux | Beside `bt3d_raylib` | Run `./bt3d_raylib` in a terminal. |
| Nintendo Switch | Beside the NRO in `sdmc:/switch/bt3d/` | Launch through Homebrew Menu. |

Keep the game and `data.pck` together. Desktop saves and settings are written
in that same folder. You do not need CMake, raylib or Docker for these downloads.
Each ZIP includes a `README.txt` with platform-specific instructions.

For the **web download**, follow its `README.txt` to serve the extracted folder
over HTTP. Put `data.pck` beside `index.html`, or choose/drop it on the page,
then click **Start**. Opening `index.html` directly does not work.

## Build from source — for developers

Download or clone this repository, copy `data.pck` next to `build.sh`, then open
a terminal in the project folder.

### macOS / Linux

Install the prerequisites in the [setup guide](docs/SETUP.md#2-install-the-tools-once),
then run:

```bash
./build.sh --run
```

This downloads raylib, builds the game, copies your assets and launches it.
The first build needs internet access. Run the same command to play again.
If your assets are elsewhere, use `./build.sh --data "/path/to/data.pck" --run`.

### Windows

Set up [Docker Desktop with WSL](docs/SETUP.md#windows-build), then run in your
Ubuntu terminal:

```bash
./build.sh --win
```

Open `build/windows/` in Windows Explorer and double-click `bt3d_raylib.exe`.
The build copies your supplied `data.pck` beside it.

### Web browser

With Docker running and Python 3 installed:

```bash
./build.sh --web
cd dist/web
python3 -m http.server 8000
```

Open `http://localhost:8000/` and click **Start**. If you did not supply assets
before building, choose or drop your `data.pck` on the page first.

### Nintendo Switch

With Docker running, build with `./build.sh --switch`, then copy
`build/switch/bt3d_raylib_nx.nro` to `sdmc:/switch/bt3d/` and launch it through
Homebrew Menu. Your supplied assets are embedded during the build.
See the [Switch guide](docs/SWITCH_PORT.md) for details.

## Unofficial project

This is an independent fan project. It is not affiliated with, authorized by,
endorsed by, or officially connected to Tibo Software. The official game website
is [Bad Toys 3D by Tibo Software](https://www.tibosoftware.com/bad-toys.htm).

Bad Toys 3D, Tibo Software, and related names, logos, artwork and game assets
belong to their respective owners. This port does not claim ownership of the
original game or its assets.
