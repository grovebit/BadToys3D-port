# Distribution builds

Run commands from the repository root. A full release requires **macOS**, CMake,
Apple command-line tools, Git and Docker. Native builds fetch raylib automatically.
See [Docker builds](DOCKER_BUILD.md) for Windows/Switch/Web prerequisites.

```bash
DATA_PCK_SRC=/path/to/data.pck MIDI_SRC=/path/to/m1.dat ./build.sh --release
```

Both asset paths are optional build inputs: `data.pck` is auto-detected as
described in the [setup guide](SETUP.md), MIDI defaults to `../original/m1.dat`. The game needs the original pack
to play; missing MIDI disables menu music. There is no soundfont build input.

## Outputs

```text
dist/macos/Bad Toys 3D.app
dist/macos/data.pck
dist/windows/bt3d_raylib.exe
dist/windows/data.pck
dist/switch/bt3d_raylib_nx.nro
dist/switch/data.pck
dist/web/index.html
dist/web/bt3d_raylib_web.js
dist/web/bt3d_raylib_web.wasm
dist/web/data.pck
dist/shared/data.pck
```

Pack copies are made only when the source exists; the Switch NRO also embeds
the pack when available at build time. The macOS pack belongs **beside the
.app**, not inside it. Scripts stage directories, not ZIP archives, and remove
legacy `macos.zip`, `windows.zip` and `switch.zip` files.

## Individual builds and restaging

```bash
./build.sh --macos                # builds and packages the app locally
./build.sh --win --switch --web   # Docker targets only; also works off macOS
./build.sh --stage-release        # copies existing artifacts; no rebuild
```

`--all` builds all four platforms, but only macOS and Web stage their outputs
as part of the build. `--release` adds the final staging pass.

`--stage-release` reads `build/windows`, `build/switch` and `build/web`.
It refreshes the app when it finds a local binary in `BT3D_MACOS_BUILD_DIR`
(default `build/macos-release`), `build-release` or `build`, in that order.
It skips missing artifacts and may leave older staged files in place; check
`dist/` before distributing. Custom Docker build paths are not used here.

Run `scripts/package-mac-app.sh` to package only an existing macOS binary;
`BUILD_DIR` overrides its preferred input directory. `--macos` builds for the
configured host architecture by default; it does not automatically produce a
universal binary.

Serve the browser bundle over HTTP as described in the [Web guide](WEB_BUILD.md).
