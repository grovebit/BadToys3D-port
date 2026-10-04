#!/usr/bin/env python3
"""Package only explicitly selected build outputs; never copy game assets."""
import argparse
import json
from pathlib import Path
import re
import shutil
import struct
import tempfile
import zipfile

COMMON = """Bad Toys 3D raylib port
https://github.com/grovebit/BadToys3D-port

GAME DATA IS NOT INCLUDED.
Provide data.pck from your own original Bad Toys 3D installation and use
this port only with a valid license for the original game. Do not rename
or unpack data.pck. Extract this ZIP before running the game.

This is an independent, unofficial fan port, not affiliated with or
endorsed by Tibo Software. Original game: https://www.tibosoftware.com/bad-toys.htm
Original game names, artwork and assets belong to their respective owners.

These builds do not embed the separate original menu MIDI asset, so menu
music is unavailable. Game sounds load from your supplied data.pck.

"""
INSTRUCTIONS = {
    'macos-universal': """MACOS (Intel and Apple Silicon, macOS 11 or newer)
1. Extract the ZIP into a writable folder.
2. Put data.pck next to Bad Toys 3D.app, NOT inside the app bundle.
3. Open Bad Toys 3D.app.
Saves and config.dat are stored beside the app. Keep the app and pack together.
The app is ad-hoc signed, not Apple-notarized. macOS may require you to approve
opening it in System Settings > Privacy & Security.
""",
    'windows-x86_64': """WINDOWS (64-bit; Windows 10/11 recommended)
1. Extract the ZIP into a writable folder.
2. Put data.pck beside bt3d_raylib.exe.
3. Double-click bt3d_raylib.exe.
Saves and config.dat are stored in this folder. No raylib installation is needed.
The executable is not Authenticode-signed.
""",
    'linux-x86_64': """LINUX (x86-64; built on Ubuntu 22.04, glibc 2.35 or newer)
1. Extract the ZIP into a writable folder.
2. Put data.pck beside bt3d_raylib.
3. In a graphical desktop terminal, run: ./bt3d_raylib
If your extractor lost permissions, first run: chmod +x bt3d_raylib
Saves and config.dat are stored in this folder. Requires X11/XWayland and
working OpenGL/audio system libraries. raylib itself is linked statically.
""",
    'switch': """NINTENDO SWITCH HOMEBREW
1. Copy bt3d_raylib_nx.nro to sdmc:/switch/bt3d/ on your SD card.
2. Put your data.pck in the same sdmc:/switch/bt3d/ directory.
3. Launch the NRO from Homebrew Menu.
The NRO contains no embedded game pack. Saves and config.dat also go in
sdmc:/switch/bt3d/. This is a homebrew build, not an eShop application.
""",
    'web': """WEB BROWSER
1. Extract the ZIP.
2. Optionally put data.pck beside index.html; otherwise select/drop it on the page.
3. Serve this folder over HTTP. With Python 3 installed, run in this folder:
   python3 -m http.server 8000
   (On Windows, you can use: py -m http.server 8000)
4. Open http://localhost:8000/ and click Start after the pack is ready.
Do not open index.html via file://. Keep the HTML, JS and WASM files together.
Saves and settings persist in browser IndexedDB for that site's origin.

Start requests fullscreen. Click during play to capture the mouse; Esc releases it.
""",
}


def package(platform, version, commit, raylib):
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", version):
        raise ValueError("Invalid release version")
    name = f"BadToys3D-{version}-{platform}"
    output = Path("dist/releases")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as temp:
        folder = Path(temp) / name
        folder.mkdir()
        if platform == "macos-universal":
            shutil.copytree("dist/macos/Bad Toys 3D.app", folder / "Bad Toys 3D.app")
        elif platform == "web":
            for source, dest in [("bt3d_raylib_web.html", "index.html"),
                                 ("bt3d_raylib_web.js", "bt3d_raylib_web.js"),
                                 ("bt3d_raylib_web.wasm", "bt3d_raylib_web.wasm")]:
                shutil.copy2(Path("build/web") / source, folder / dest)
        else:
            source = {"windows-x86_64": "build/windows/bt3d_raylib.exe",
                      "linux-x86_64": "build/ci/bt3d_raylib",
                      "switch": "build/switch/bt3d_raylib_nx.nro"}[platform]
            if platform == "switch":
                data = Path(source).read_bytes()
                aset = struct.unpack_from("<I", data, 0x18)[0]
                if data[aset:aset + 4] != b"ASET" or struct.unpack_from("<Q", data, aset + 0x30)[0] != 0:
                    raise ValueError("Switch release must have no embedded RomFS")
            shutil.copy2(source, folder / Path(source).name)
        (folder / "README.txt").write_text(COMMON + INSTRUCTIONS[platform] +
                                          f"\nVersion: {version}\nSource commit: {commit}\n")
        (folder / "BUILD_INFO.json").write_text(json.dumps({
            "version": version, "source_commit": commit, "platform": platform,
            "game_data_included": False, "embedded_menu_midi": False,
        }, indent=2) + "\n")
        licenses = folder / "LICENSES"
        licenses.mkdir()
        shutil.copy2(raylib / "LICENSE", licenses / "raylib.txt")
        shutil.copy2(raylib / "src/external/glfw/LICENSE.md", licenses / "GLFW.txt")
        tml = Path("src/third_party/tinymidiloader/tml.h").read_text()
        (licenses / "TinyMidiLoader.txt").write_text(tml.split("*/", 1)[0] + "*/\n")
        for path in folder.rglob("*"):
            if path.name.lower() in {"data.pck", "m1.dat", "config.dat", "savegame.dat"}:
                raise ValueError(f"Unexpected game data in package: {path}")
        archive = shutil.make_archive(str(output / name), "zip", temp, name)
        with zipfile.ZipFile(archive) as zipped:
            if zipped.testzip() is not None:
                raise ValueError("Archive integrity check failed")
        print(archive)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=INSTRUCTIONS)
    parser.add_argument("version")
    parser.add_argument("commit")
    parser.add_argument("raylib", type=Path)
    args = parser.parse_args()
    package(args.platform, args.version, args.commit, args.raylib)
