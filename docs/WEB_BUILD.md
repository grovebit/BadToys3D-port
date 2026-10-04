# Browser/WASM Build

The browser port is built from the C/raylib runtime with Emscripten. It shares the desktop runtime in `src/`.

## Build

Requires Bash and a running Docker daemon; run from the repository root.
See [Docker configuration](DOCKER_BUILD.md) for image and path overrides.

```bash
./build.sh --web
```

Output is staged in:

```text
dist/web/index.html
dist/web/bt3d_raylib_web.js
dist/web/bt3d_raylib_web.wasm
dist/web/data.pck        # optional, copied when DATA_PCK_SRC exists
```

The build uses `docker/Dockerfile.web` and `scripts/build-web-docker.sh`.
`build.sh` shares [data discovery](SETUP.md#3-add-the-game-data-and-play) with
other targets; use `--data /path/to/data.pck` to choose a pack explicitly.

## Run Locally

Serve the directory over HTTP:

```bash
cd dist/web
python3 -m http.server 8000
```

Then open `http://127.0.0.1:8000/`. If `data.pck` is next to `index.html`,
the shell loads it automatically; otherwise choose or drop the original
`data.pck`. Click Start after the pack is ready. Do not use `file://`.

The web main loop uses `requestAnimationFrame`; presentation rate depends on
the browser, OS and display refresh rate.

## Persistence

The web shell mounts `/bt3d` as IDBFS before starting the game. Saves and
`config.dat` are written there and synced to IndexedDB after successful writes.
The shell first tries to fetch `data.pck` from the same directory as
`index.html`. If that file is unavailable, the selected `data.pck` is loaded
into the in-memory Emscripten filesystem as `/data.pck` for that browser
session. The first screen keeps the Start button so browser audio can be
initialized from a user gesture.
