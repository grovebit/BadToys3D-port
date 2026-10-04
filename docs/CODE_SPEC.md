# Raylib Port Code Specification

This document describes the structure and behavior of the `raylib-port` codebase.

## Scope

The port is a native C11/raylib runtime for the original Bad Toys 3D data files.
It does not add content. It is driven by three things:

- `data.pck` for all authored assets and levels (`MAP_*` entries)
- raylib 5.5 on desktop and web, and raylib-nx 4.2 on the Nintendo Switch
- libnx on the Switch, where gamepad input is read directly

All game state lives in one `AppState` (`src/bt3d_app_state.h`). This includes
everything that outlives a frame:
- the session's difficulty;
- input edge and key-repeat history;
- overlay animation;
- the debug frame graph.

Only per-frame scratch buffers in the renderers and the profiler stay static.
Modules take `AppState` as a parameter, and read-only functions take
`const AppState *`.

## Frame Flow

`runtime.c` holds startup, one frame and shutdown (`bt3d_runtime_*`), so every
entry point shares them. `main.c` is the entry point for desktop, web and the
Switch, and the Android port reuses the same runtime. Desktop builds loop. The
web build runs the frame from `emscripten_set_main_loop_arg`, paced by
`requestAnimationFrame`.

1. `bt3d_read_frame_input` samples the keyboard, mouse and gamepad once
   into a `FrameInput` (`input.c`). This is the only place platform input is
   read.
2. `bt3d_update_player` (`player.c`) does one of three things:
   - runs the menu (`menu.c`) when it is open;
   - otherwise runs a death or level transition (`player_flow.c`);
   - otherwise runs gameplay: look, actions, movement, timers, doors,
     visibility, projectiles, enemies and ambient sound.
3. `bt3d_update_menu_music` keeps the menu MIDI playing while the menu is open.
4. `bt3d_draw_frame` (`frame.c`) draws one of these:
   - the missing-data screen;
   - the menu;
   - a transition;
   - the automap;
   - the world, then the HUD and weapon.

Startup order:
1. Reset `AppState`.
2. Move the working directory next to the executable or macOS app bundle (desktop).
3. Load the config.
4. Create the window and audio device; this applies the display mode once.
5. Set up input.
6. Load the data pack and assets.
7. Open the main menu.

## Modules

| File | Role |
| --- | --- |
| `main.c` | the platform entry point and frame loop |
| `runtime.c` | startup, one frame and shutdown for every entry point |
| `input.c` | `FrameInput`: per-platform bindings, mouse edges, menu key repeat, mouse capture |
| `platform.c` | save directory and paths, data-pack candidates, window/audio init, display mode, config file |
| `datpack.c` | reads `data.pck` into memory, validates it and sorts entries for binary search |
| `assets.c` | decodes every texture, bitmap and sound at startup into a per-entry table |
| `audio.c` | plays sounds by original sound id |
| `midi_player.c` | menu music |
| `map242.c` | parses `MAP_*` entries: tiles, details, events (doors), markers, objects |
| `campaign.c` | campaign map order |
| `session.c` | loads and starts maps, builds per-map caches, new game, map cycling, level transitions |
| `world_spawn.c` | fresh player state and spawn placement |
| `world_tiles.c` | tile queries, declared in `bt3d_world_grid.h`, `_classify.h`, `_collision.h`, `_doors.h` and `_textures.h` |
| `world_markers.c` | marker positions and hanging props |
| `world_overlays.c` | animated wall and door overlays |
| `visibility.c` | line of sight and the visible, render and discovered tile masks |
| `doors.c` | the door state machine |
| `pickups.c` | the pickup table and its effects |
| `player*.c` | gameplay update, movement and collision, use and pickups, weapons, transitions |
| `combat.c` | player fire, projectiles, explosions, damage, enemy drops |
| `weapons.c` | player weapon table and animation timing |
| `enemies.c`, `enemy_ai.c` | enemy classes, difficulty and animation; chase, doors and attacks |
| `render_world.c` | floors, ceilings, walls, doors and overlays |
| `render_billboards.c` | markers, enemies, projectiles and impacts as sprites |
| `render_ui.c` | HUD, weapon viewmodel, transition and damage overlays |
| `map_overlay.c` | the original automap |
| `menu.c`, `menu_draw.c` | menu pages, input and actions; menu drawing |
| `frame.c` | per-frame draw composition |
| `savegame.c`, `save_slots.c` | save file format; quicksave, named saves and the load list |
| `debug_tools.c`, `profiler.c` | debug overlay and clipboard text; optional zone profiler |

## Assets

`bt3d_load_assets` decodes the whole pack once, in this order:

1. `BT_PAL`, the palette.
2. `STN_*` 64x64 wall, door, floor and ceiling textures.
3. `VEC_*` patches: props, overlays, pickups and weapon frames.
4. Enemy and projectile sprite families.
5. `BM_*` and `M_*` bitmaps: 4- and 8-bit BMP, raw or RLE, with the blue key
   made transparent.
6. `SND_*` sounds.

Each texture is stored as a `PackTexture`, indexed by its pack entry. It
records its opaque bounds, which billboards use to crop themselves and hanging
props use to place themselves. Mirrored and split door variants are drawn by
flipping or slicing UVs rather than as extra textures.

## World

The map is a 64x64 tile grid, stored column-major (`index = x * 64 + y`). Bit
`0x8000` marks a solid tile and `0x2000` a structural wall. Events are doors;
markers are pickups or props; objects are enemies and the start position.

When a map is loaded, `session.c` builds per-map caches:
- the door index per cell;
- the ceiling-marker mask;
- the structural-wall mask;
- the raw floor and ceiling texture codes.

### Visibility

`bt3d_refresh_visible_tile_mask` marks the 3x3 block around the player. It then
casts a fan of 1024 grid rays that stop at walls and closed doors. From that:
- visible cells and their neighbours become render cells;
- visible cells, and the doors and walls next to them, become discovered for
  the automap.

The result is cached by player position and `doors_version`.
`doors_version` is a counter that `doors.c` bumps whenever a door starts or
stops blocking its cell. Line of sight between two points is an exact grid
traversal (DDA).

### Rendering

`render_world.c` turns each render cell inside the horizontal view cone into
textured quads:
- floors and ceilings for open cells;
- the exposed faces of structural walls;
- door boxes or split leaves.

Quads are counting-sorted by texture and drawn with one `rlBegin` per
texture. Wall and door overlays come next, with depth writes off. Billboards
come after that, sorted far to near, also without depth writes. The HUD and
the weapon are drawn in screen space.

## Gameplay

- **Player:**
  - movement is tile collision, plus a check against living enemies;
  - debug noclip skips both.
- **Doors:**
  - each door opens, waits, holds open while occupied, then closes;
  - keyed doors need the matching key;
  - split doors open as two leaves.
- **Enemies:**
  - an enemy aggros when its cell is visible to the player;
  - it chases along a breadth-first distance field to the player's cell;
  - the field is rebuilt only when the player changes cell or `doors_version`
    changes;
  - enemies open unkeyed doors on the way, and attack when in range with
    direct line of sight;
  - difficulty scales health, damage and cooldowns.
- **Death:**
  - the current level restarts, and the player keeps the fist and pistol;
  - with no lives left, the game restarts at level 1 with a fresh player.
- **Level exit:**
  - a transition card is shown, then the next map starts;
  - keys are cleared and the game is quicksaved.

## Input

`FrameInput` holds three kinds of input:
- held values: move, turn, look stick, mouse delta;
- pressed edges: fire, use, map, pause and the menu actions;
- debug shortcuts, which act only while debug tools are on.

Bindings by platform:
- **Desktop:** keyboard, mouse, and the first connected gamepad.
- **Switch:** the libnx pad.

Menu directions repeat every 0.2 s while held.

## Menus

`menu_build_page` describes the current screen as a list of `MenuItem`s, each
with an action, a label and a rectangle. Update and drawing both use this
page, so hit testing always matches what is drawn.

On the load screen:
- the list scrolls in a window of five;
- Backspace, or the left face button, deletes the selected save.

Opening the menu always starts on the main screen at the first item.

## Tests and Ports

`tests/` holds parser regression tests for `datpack.c` and `map242.c`, plus a
headless gameplay/save harness on native macOS/Linux. Enable them with
`-DBT3D_BUILD_TESTS=ON`; see the [test commands](../README.md#tests). Code that
is only for desktop builds is guarded by `BT3D_PLATFORM_SWITCH`, `_WEB` and `_ANDROID`.
`android/` is the Gradle scaffold for the Android port; see
[the Android plan](ANDROID_PORT_PLAN.md).

## Saves and Config

Save format **3** stores a `SaveHeader` followed by the raw door, marker, enemy and
projectile arrays. The header holds the magic, the format version, the map
index, the difficulty, `PlayerState`, the array counts and the discovered
mask. Saves of any other format version are ignored. `savegame.dat` is the
quicksave, written by F5, by every named save and at each level exit. Named
saves are `save-<name>.dat`.

Config format **5** stores `{ magic, version, AppSettings }` in `config.dat`.
Values are clamped on load, and the file is rewritten when the options screen
is closed.

The save directory depends on the platform:
- **Desktop:** beside the executable, or beside the macOS app bundle.
- **Web:** `/bt3d`, an IDBFS mount synced after every write.
- **Switch:** `sdmc:/switch/bt3d`.

Save writes use a sibling `.tmp` file and replace the destination after a
successful write and close. Failed writes retain the old save; failed loads
retain the active game state. This is not a power-loss durability guarantee.
Raw struct storage also means saves are not portable across builds with
different struct layouts. Web persistence additionally depends on IndexedDB sync.
