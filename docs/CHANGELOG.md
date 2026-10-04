# Raylib Port Changelog

Historical record of changes across the port. Earlier entries describe the
code at that time; see [current release notes](RELEASE_NOTES.md) for present behavior.

## Simplification pass (October 2026)

The code was restructured for simplicity and speed, with no backward
compatibility kept. `src/` went from 86 files and 10.1k lines to 70 files and
7.6k lines.

### Structure
- **Assets:** every texture, bitmap and sound is decoded once at startup into
  a table indexed by pack entry. This replaced the lazy slot caches and the
  per-map prewarm (`prewarm.c`). `data.pck` is read into memory once and looked
  up by binary search.
- **World rendering:** one module (`render_world.c`) gathers the quads of the
  visible cells, culls them to the view cone, sorts them by texture, and draws
  opaque geometry before overlays.
- **Visibility:** cached by player position and a door-change counter. Line of
  sight is an exact grid traversal.
- **Enemy chasing:** the distance field is rebuilt only when the player changes
  cell or a door opens or closes.
- **Input:** read once per frame into a `FrameInput` (`input.c`). All platform
  `#if`s for input live there.
- **Menus:** built from one page description (items, labels, rectangles) that
  update and drawing share.
- **Startup:**
  1. Load the config.
  2. Create the window; the display mode is applied once.
  3. Load the pack.
- **Removed:** the status-text system, the native missing-data dialogs and
  `platform_win_dialog.c`.
- **Saves:** format 3. **Config:** format 5, which stores `AppSettings`
  directly. Older files are ignored.
- **Builds:** Release by default. The web build runs on `requestAnimationFrame`
  without ASYNCIFY. The Switch `.nro` rebuilds when its inputs change.

### Behavior changes
- **Walls:**
  - the ±X faces now use the same texture orientation as the other faces;
  - floors are no longer drawn under walls.
- **Doors and overlays:**
  - split door leaves are drawn at the door's near face;
  - wall and door overlays draw after opaque geometry without depth writes;
  - overlays of fully open doors are skipped;
  - overlay animation runs on the game clock, so it pauses with the menu and
    restarts with each map.
- **Status text:** status messages are gone ("Key N required", "Loaded …",
  "Checkpoint …").
- **Map loading:** projectiles, impacts, the automap, transitions and the
  damage flash all reset when a map loads.
- **Missing `data.pck`:** shown as an in-game message on every platform.
  Desktop builds no longer exit straight away; Esc quits.
- **Debug shortcuts:** the profiler (F10) and clipboard copy (C) now need
  debug tools, as noclip already did. The d-pad map cycle steps once per
  press.
- **Menu navigation:**
  - menu directions repeat while held, on every platform;
  - the Switch menus also take the left stick;
  - Tab no longer moves down.
- **Menu mouse:**
  - hovering selects an item only when the mouse moves;
  - clicking outside every item does nothing;
  - volume sliders are grabbed by the slider, not the label.
- **Menu layout:**
  - the options labels drop the `> ` marker and the `[Left/Right]` suffix;
  - the save dialog's name field and buttons sit on the menu grid.
- **Mouse sensitivity:** limited to 0.2–3.0 everywhere.

### Fixes
- Resuming from the menu with a mouse click no longer fires a shot.
- Desktop gamepads no longer move backward when the left stick is pushed
  forward.
- Gamepad turning no longer depends on the frame rate. It was about 2.4x
  faster at 144 fps than at 60.
- Typing W, A, S, D or a space into a save name no longer moves the selection
  or confirms the dialog.
- Clicks in the save dialog hit the button that is drawn, and the hint no
  longer overlaps Cancel.
- The menu opens on its first item after a game is loaded. Previously the
  stale selection could leave "Exit to Menu" selected.
- Holding the d-pad with debug tools on no longer cycles maps every frame.
- Y deletes saves on the Switch; the pad is now read through libnx.
- A slider drag no longer jumps to the neighbouring slider.

## Nintendo Switch Port

### Build System
- Added `cmake/switch-toolchain.cmake` for devkitPro/aarch64 cross-compilation
- Updated `CMakeLists.txt` with Switch linking (EGL, GLESv2, glapi, drm_nouveau, nx, stdc++), PLATFORM_NX/GLES2 defines, and .nro packaging via elf2nro with embedded romfs
- Added `-O3` optimization flag to the Switch toolchain (required for acceptable performance)
- Added `build-switch-docker.sh` for Docker-based builds without local devkitPro
- Added `romfs/` directory for embedding `data.pck` into the .nro
- NRO icon from `bad_toys_2.jpeg` (256x256 JPEG embedded via `--icon`)
- Added top-level `build.sh` as the main build entrypoint:
  `--win`, `--switch`, `--all`, `--release`, `--clean`, and
  `--docker-image`. The entrypoint is Docker-only; host-native desktop and
  macOS bundle builds are intentionally not part of this wrapper.
- `scripts/build-all.sh` is now a compatibility wrapper around
  `build.sh`, preserving the old comma-separated Docker target interface:
  `all`, `win`, `switch`, and `win,switch`.
- Added `docs/DOCKER_BUILD.md` with the command reference, Docker image
  behavior, `data.pck` handling, external-drive staging, release artifacts,
  and lower-level script notes.

### Platform Layer
- Added `romfsInit()`/`romfsExit()` guards in `main.c` for Switch
- Added `socketInitializeDefault()`/`nxlinkStdio()` for nxlink debug output
- Guarded `DisableCursor()`/`EnableCursor()` calls (no cursor on Switch)
- Added save directory creation (`sdmc:/switch/bt3d/`) on startup
- Gamepad index always returns 0 on Switch (bypasses raylib detection)
- `SetTargetFPS(60)` on all platforms (removed vsync-only mode)

### Controls
- Left stick: move
- Right stick: look (quadratic response curve for precision, base multiplier 28)
- D-pad: move (gameplay) / navigate (menus)
- ZR: fire
- ZL / A: use / interact
- L: cycle weapon prev
- R: cycle weapon next
- X: map
- +: menu / pause
- B: back (menus)
- Y: delete save (load screen)
- D-pad left/right: cycle maps (debug tools only)
- Left stick Y axis negated for Switch (libnx uses Y-up convention)
- Right stick Y axis negated for Switch
- Desktop right mouse now triggers interact/use.
- Desktop mouse fire/interact are edge-detected so holding a mouse button
  does not repeatedly retrigger one-shot actions.

### Menu
- Direct libnx pad reading (`padGetButtons`/`padUpdate`) in `menu.c` bypasses raylib's broken gamepad `ready` check
- D-pad only for menu navigation on Switch (sticks suppressed)
- All raylib keyboard emulation suppressed on Switch (`MENU_KEY_*` returns 0, confirm/back skip `IsKeyPressed`)
- Only libnx `HidNpadButton_A/B/Plus/Minus` handle confirm/back on Switch
- Cooldown timer (0.2s) for d-pad repeat in menus
- Menu open cooldown (0.15s) prevents + button from immediately closing the menu
- Switch footer hints show button names (A/B/Y/D-Pad) instead of keyboard keys
- Save dialog skips text input on Switch (uses auto-generated name)

### Performance Optimization
- **Floor/ceiling batching**: Replaced individual `DrawModelEx` calls with `rlBegin(RL_QUADS)` batched rendering sorted by texture. Reduced CPU draw calls from ~1200 to ~10-20 for floor/ceiling surfaces. The Switch was 99% CPU bound on draw call overhead, 0% GPU.
- **LOS-based rendering**: Only tiles in the player's line of sight are rendered (walls, floor, ceiling, sprites). No artificial distance cap. Visibility mask uses 256 rays × 96 steps (covers full 64x64 map).
- **Separate render vs gameplay visibility**: `render_tile_mask` extends `visible_tile_mask` by 1 tile in all directions so wall faces at visibility edges render. `visible_tile_mask` remains unextended for enemy aggro/LOS checks.
- **Sprite culling**: Markers and enemies on non-visible tiles are skipped entirely.
- **Enemy AI culling**: Enemies beyond 22 tiles skip full AI (LOS, pathfinding, chase). Only timers and animation tick.
- **Chase BFS skip**: The expensive BFS distance field only builds if an aggroed enemy is within 22 tiles.
- **Ceiling winding fix**: Floor quads face up, ceiling quads face down (reversed winding order).
- **Billboard profiling**: Added sub-zones under `draw_billboards`
  (`db_markers`, `db_enemies`, `db_enemy_proj`, `db_sort`,
  `db_draw`) to separate gather/sort/draw cost on Switch.
- **Projectile billboard culling**: Enemy projectiles outside
  `render_tile_mask` are skipped before texture lookup, sorting, and
  drawing.
- **Patch prewarming**: Map load now preloads patch textures for map
  markers, enemy families, enemy projectile families, wall overlays,
  and door overlays, including animated overlay frame variants.
- **Patch cache expansion**: `patch_textures` capacity increased from
  256 to 768 entries so map-level prewarming can keep normal, mirrored,
  and split patch variants resident.
- Added flat floor plane model (2 triangles) replacing cube model (12 triangles) for floor/ceiling.
- Walls still use individual `DrawModelEx` (~250 calls, acceptable).

### Gameplay / Rendering Polish
- Player movement now collides with living enemies, while still allowing
  the player to move out of an existing overlap and preserving noclip.
- Player direct-fire weapons now use first-impact hitscan: the shot ray
  finds the nearest wall/closed-door collision and only damages an enemy
  if its ray intersection happens before that blocker.
- Direct-fire shots now draw a short PSK impact animation at the traced
  collision point for both wall/door misses and enemy hits.
- Level transitions now draw the original `BM_POD` bitmap background behind
  the `BM_LEVEL` label and level digits as their own screen instead of
  dimming/rendering the live gameplay frame.
- The startup menu now uses the same original `BM_POD` blue patterned
  background as level transitions and plays original `m1.dat` MIDI on the
  startup main-menu screen through an embedded MIDI and built-in synth stream
  path, with no loose `m1.dat` or `soundfont.sf2` needed at runtime.
- Dead enemy billboards crop away sparse stray pixels above the visible
  corpse body, fixing occasional dark/brown lines during death animation.
- The centered player HUD no longer draws a dark full-width continuation
  behind the original `BM_LISTA` panel.
- Doors kept as individual `DrawModel` (few per frame).
- Removed `DrawCubeWires` debug outlines from doors.
- Current Switch profiling after the prewarm pass still shows some
  active gameplay texture and wave loads. `draw_billboards` is usually
  low (`~0.06-0.17ms`) but still spikes through `db_markers` when a
  missing `VEC_*` is first touched. `update_player` / `events` spikes
  also correlate with lazy `WAVE` loads. Next target is broader asset
  prewarming rather than geometry.

### Rendering Changes
- Door overlays (outline/frame decorations) now render on both sides of door leaves, matching the browser port. The `door_overlay_index_for_cell` function was previously defined but never called — overlays are now drawn in Pass 2 using the VEC patch textures from `detail_grid[base+1]`, animated via `animated_overlay_id`, and slide with the door.
- Removed death transition bitmap slideshow (`BM_ES1`-`BM_ES4`), replaced with red fade overlay
- Doors now render at full wall height instead of 86%
- Doors block visibility only when fully closed (`openness <= 0`), not at 50% — eliminates black spots during door transitions

### Audio
- Door close sound (`SND_23`) plays when a door finishes closing
- Ambient sound system matching the browser/Go port — randomly plays from pool (`SND_16, 17, 30-36, 38-41`) every 45-60 seconds, resets per map, avoids repeating same sound

### Game Logic
- Texture unloading between map transitions (wall, patch, bitmap textures freed)
- No startup map preload — maps load only on New Game or Load Game
- Per-overlay animation state matching Go/browser port (mode 0 = regular cycle, mode 1 = random 10% flicker)
- Animation state resets on map load
- Enemy hit stun: 0.4s hurt window where attack cooldown ticks at 20% speed (not full reset)
- Keys reset to zero on level transition
- All three render models (cube, floor plane, wall plane) initialized at startup
- Guard rendering behind `in_session` check (prevents crash when no map loaded)

### Save System
- Save names follow "Level N - YYYY-MM-DD HH:MM" format
- Save list labels read level number from save file header
- Labels show filename only, not full path
- Save directory created automatically on Switch
- Config file (`config.dat`) persists mouse sensitivity and debug tools setting
- Config loaded at startup, saved when leaving Options screen

### Debugging
- File-based `debug_log()` function writing to `sdmc:/switch/bt3d/debug.log` (or `./debug.log` on desktop)
- Perf stats logged every second during gameplay (FPS, map, position, entity counts)
- Detailed load-game logging with step-by-step status and struct size reporting

### Enemy Count Verification
Original enemy counts per map verified against `data.pck`:
- Level 1 (MAP_19): 6 enemies
- Level 2 (MAP_18): 42 enemies
- Level 3 (MAP_17): 76 enemies
- Later levels: 45-146 enemies (original game design)
- Import filter matches Go/browser port exactly: `class_id <= 5`, skip spawn marker

### Known Issues
- nxlink real-time logging not working (network discovery issue)
- Save files are binary and cross-platform incompatible (struct padding differs x86_64 vs aarch64)
- Wall batching via rlgl not implemented yet (winding/texture issues deferred)

## Desktop Polish & Distribution

Changes made after the Switch port stabilized, focused on visual fidelity to
the original game and shipping the desktop build standalone.

### World scale and projection
- Wall faces now render as 1.0 x 1.0 squares (`WALL_HEIGHT 1.6f -> 1.0f`).
  Cells are 1.0 wide so this matches the original cube proportions.
- `PLAYER_HEIGHT` lowered to `0.55f` so the floor occupies a slightly larger
  share of the view, matching the original eye position.
- Camera `fovy` set to `60.0f` (from 58); horizontal FOV at 16:9 reads
  closer to the original software renderer.
- `MARKER_BILLBOARD_SIZE` and `ENEMY_BILLBOARD_SIZE` set to `1.0f`
  (full tile) so crates/enemies match the wall scale.
- Weapon viewmodel rebalanced: `8.0f` height-relative scale, 0.9x on X and
  1.15x on Y so the patch retains the 320x200 non-square pixel aspect of the
  original VGA art. Bottom anchor now flush with the screen edge.

### Door overlays / wall trims
- Door-frame wall-overlay planes now render full-size (1.0 x WALL_HEIGHT)
  instead of 0.9 x 0.9 of the wall, with `outward 0.015` to avoid both
  z-fighting and visible separation from the wall at shallow angles.
- Pass 3 wall overlays no longer require the wall cell itself to be in the
  render mask — they additionally render when the face borders an in-mask
  door cell, so the trims around a visible door appear at first sight
  rather than after one extra step.
- Door overlays (placards/decals on door faces) sit at `0.18*0.5 + 0.005`
  outside each door face with split-leaf support via
  `get_split_patch_texture`, so animated VEC art tracks the correct half
  of a center-split door.

### Death / lives behavior
- Player death now reloads the current map fully (enemies, doors,
  markers, projectiles) instead of just respawning at the spawn point.
  Health restores to 99 and the upgraded weapons (slots 3/4) are stripped,
  leaving the starting gun.
- When the last life is lost the death transition still plays, but on
  completion `init_player_state` runs and level 1 is loaded — the run
  restarts from scratch instead of returning to the main menu.

### PC input (`!BT3D_PLATFORM_SWITCH` only)
- Up/Down arrow walks forward/back alongside W/S; A/D continues to strafe.
- Left/Right arrow turns the view (yaw) at 2.4 rad/s alongside the mouse.
- Left Ctrl fires alongside left mouse click.
- The map overlay now also closes on any arrow key press.

### PC packaging
- On startup the desktop build does
  `ChangeDirectory(GetApplicationDirectory())`, so the binary works no
  matter how it was launched (Finder, double-click from a copied folder,
  etc.).
- `bt3d_get_data_pack_candidates` now points at a single user-facing path:
  `data.pck` next to the executable. Dev-tree fallbacks remain for in-repo
  runs.
- A dedicated "data.pck not found" screen is drawn each frame when no
  pack loaded, telling the player the exact directory to drop the file in.
- `CMakeLists.txt` gained two desktop options:
  - `BT3D_STATIC_RAYLIB=ON` — links `libraylib.a` instead of the dylib
    so the released binary has no raylib runtime dependency.
  - `BT3D_RAYLIB_ROOT=<dir>` — point at an external raylib install
    (`{include,lib}` layout). Useful with the upstream
    `raylib-5.5_macos.tar.gz` universal lib.
- Together with `CMAKE_OSX_ARCHITECTURES="x86_64;arm64"` this produces a
  ~3 MB universal macOS binary that runs on Intel and Apple Silicon with
  only system frameworks linked.
- Strict pack lookup: removed the dev-tree fallbacks; `data.pck` must sit
  next to the executable.
- Native OS error dialog if the pack is missing: macOS uses
  `osascript`, Linux uses `zenity`/`kdialog`. The window opens, the
  loader fails, the dialog explains where to drop the file, then the
  game closes the audio device and the window and exits cleanly. The
  in-game text screen has been removed.

### Difficulty system
- Added Easy / Medium / Hard difficulty. Each multiplies four enemy
  stats:
  - HP: 0.6 / 1.0 / 1.4
  - attack cooldown: 1.5 / 1.0 / 0.7
  - initial reaction delay: 4.0 / 2.5 / 1.2
  - damage: 0.6 / 1.0 / 1.4
  Medium reproduces the pre-existing balance for HP, cooldown, and
  damage but applies the new initial-reaction multiplier.
- Difficulty lives on `AppState` and is mirrored into a module-local
  in `enemies.c` via `bt3d_set_difficulty` so the per-class stat
  helpers can apply the multipliers without threading `AppState`
  through.
- New Game now opens a `MENU_SCREEN_DIFFICULTY` panel
  (Easy / Medium / Hard / Back) before starting; the chosen value is
  applied before `start_new_game` runs.
- `EnemyRuntime` gained `had_los_last_tick`. When the player breaks
  LOS and later re-enters it the initial-attack-delay is re-applied
  so enemies can't snap-shoot through cover; aggro itself is preserved
  while hidden so chase behaviour continues uninterrupted.
- `SaveHeader` gained `int difficulty` and `SAVE_FORMAT_VERSION` is
  bumped to 2 (older saves are rejected). Save labels in the load
  menu are now `Level N [Easy/Medium/Hard] - YYYY-MM-DD HH:MM`.
