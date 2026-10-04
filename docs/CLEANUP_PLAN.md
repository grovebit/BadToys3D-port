# Cleanup Plan

This cleanup is intentionally staged so source movement stays easy to review and behavior remains stable.

## Guardrails

- Keep gameplay behavior unchanged unless a later task explicitly requests behavior changes.
- Prefer moving existing functions unchanged before refactoring their internals.
- Verify each pass with a host build at minimum.
- Preserve dirty worktree changes that are not part of the active cleanup task.

## Verification

- Host configure/build: `cmake -S . -B build/macos-release` then `cmake --build build/macos-release`.
- Release build when assets are available: `DATA_PCK_SRC='../original/data.pck' MIDI_SRC='../original/m1.dat' ./build.sh --release`.
- Platform checks when the toolchains are available: Docker Windows, Web, and Switch builds.
- Smoke run with `data.pck` after source movement that affects loading, session flow, rendering, or gameplay.

## Backlog

- Consider whether the startup asset decoder in `assets.c` needs splitting.

## Completed Passes

- **Startup:** every texture, bitmap and sound is decoded once at startup.
  This replaced the slot caches and the per-map prewarming.
- **World rendering:** one renderer culls to the view cone, sorts quads by
  texture, and draws overlays after opaque geometry.
- **Caching:** visibility and the enemy chase field are cached until the
  player changes cell or a door changes.
- **Input:** read once per frame into `FrameInput` (`input.c`).
- **Menus:** built from one page description that update and drawing share.
- **State:** state that outlives a frame lives in `AppState`. This covers the
  session's difficulty, input history, overlay animation and the debug frame
  graph.
- Split `bt3d_world.h` into focused public headers and converted source files to include the narrow world APIs they use.
- Removed the temporary `bt3d.h` and `bt3d_world.h` umbrella headers.
- Split the broad `bt3d_world_tiles.h` API into focused grid, classification, collision, door, and texture headers.
- Moved raylib, desktop, web, and Switch CMake target configuration into focused helper files.
- Narrowed the remaining public headers that only needed forward declarations or smaller type headers.
- Split `world_state.c` into focused world modules for feedback, markers, overlays, runtime allocation, spawn setup, and tile/blocking logic.
- Moved player shortcut execution into `player_shortcuts.c`.
- Named remaining local game-data IDs in audio, pickups, doors, and tile classification.
- Moved desktop data-pack working-directory and missing-file reporting into platform helpers.
- Moved source lists into `cmake/sources.cmake`.
- Prefixed broad world helper APIs with `bt3d_` for clearer ownership.
- Named local AI/input tuning constants and split `enemy_ai.c` / `player_input.c` update paths into smaller helpers.
- Made runtime VEC remap definitions self-contained so asset IDs, animation mode, frame tables, and counts live in one table.
- Moved `AppState` and related state structs into `bt3d_app_state.h`.
- Split save declarations into `bt3d_savegame.h` and `bt3d_save_slots.h`; kept save-format details in `bt3d_save_internal.h`.
- Narrowed `bt3d_platform.h` and `bt3d_campaign.h` to forward declare `AppState`.
- Extracted campaign map ordering from `session.c`.
- Extracted resource prewarming from `session.c` (later removed in favor of startup decoding).
