# Nintendo Switch Porting Notes

Historical record of the porting process, including superseded experiments,
build commands and behavior. For current setup use the [README](../README.md),
[Docker guide](DOCKER_BUILD.md) and [code specification](CODE_SPEC.md).

## 1. Build System Setup

### Steps
1. Installed devkitPro via `.pkg` installer (Homebrew tap failed due to git auth)
2. Installed `switch-dev` via `dkp-pacman`
3. Missing `switch-mesa` and `switch-libdrm_nouveau` — GLES2/EGL headers not found
4. Cloned `raylib-nx` from GitHub

### Problem: raylib-nx build failed on `raudio.c`
- **Error**: `pthread_attr_setschedpolicy`, `sched_get_priority_min/max` — implicit declaration
- **Root cause**: libnx declares but doesn't implement these POSIX scheduling APIs. miniaudio references them in thread creation.
- **Solution**: `switch-nx/dlfcn.h` already had stub implementations, but they were defined as regular functions (causing redefinition errors) and `pthread_attr_t` wasn't declared (no `#include <pthread.h>`).
- **Fix**: Added `#include <pthread.h>` to `dlfcn.h`, made stubs `static inline`, and added `#include "switch-nx/dlfcn.h"` in `raudio.c` BEFORE `#include "external/miniaudio.h"` so stubs are available when miniaudio compiles.

### Problem: CMake `find_package(raylib)` failed for Switch
- **Root cause**: raylib-nx was installed manually (just `.a` + headers), no CMake config files.
- **Fix**: On Switch, skip `find_package` and link `libraylib.a` directly with explicit include paths.

### Problem: Link errors — `__cxa_guard_acquire`, `operator delete`, `std::__detail::_List_node_base`
- **Root cause**: `libEGL.a` and `libdrm_nouveau.a` are compiled from C++ (Mesa), but we link with the C compiler.
- **Fix**: Added `-lstdc++` to Switch link libraries.

### Problem: `elf2nro --romfs=romfs` failed with "Failed to open input romfs"
- **Root cause**: `--romfs` expects a pre-built romfs IMAGE file, not a directory.
- **Fix**: Changed to `--romfsdir=romfs` which builds the romfs from a directory.

## 2. Performance

### Problem: 1 FPS on Switch
- **Initial state**: Game rendered all 4096 cells (64x64 map) every frame. Each cell = 2 `DrawModelEx` calls (floor + ceiling) + wall faces. Total ~8000+ draw calls per frame.
- **Diagnosis**: Switch reported 99% CPU, 0% GPU — the bottleneck was CPU-side draw call overhead, not GPU rendering.

### Attempted fix: Distance culling (render_dist = 14-18 tiles)
- Reduced cells from 4096 to ~600, but FPS was still only ~20-24 on heavy maps.
- **Lesson**: Draw calls, not cell count, was the bottleneck. Each `DrawModelEx` has massive CPU overhead on Switch GLES2.

### Attempted fix: Flat floor plane model (2 triangles vs 12 for cube)
- Replaced `DrawModelEx` with cube model for floor/ceiling with a custom flat plane.
- Reduced triangle count 6x per tile but still same number of draw calls.
- **Lesson**: Triangle count wasn't the bottleneck — draw call count was.

### Attempted fix: rlgl batched floor/ceiling quads
- First attempt: floor/ceiling quads rendered but textures were missing.
- **Root cause**: Wrong winding order — quads faced away from camera.
- Fixed winding, added `rlDisableBackfaceCulling()` as safety net.
- FPS counter showed 1800 but the scene was broken (textures on wrong face).
- **Lesson**: High FPS with broken rendering = false positive.

### Attempted fix: rlgl batched walls
- Walls rendered invisible even with backface culling disabled.
- Multiple attempts to fix vertex positions and winding failed.
- **Decision**: Abandoned wall batching. Wall draw calls (~250) are acceptable. Only floor/ceiling batching (~1200 draw calls → ~10-20) was needed.

### Working solution: `-O3` + batched floor/ceiling + distance culling
- `-O3` compiler flag was essential (not optional).
- Floor/ceiling batched via `rlBegin(RL_QUADS)` sorted by texture — one draw call per unique texture.
- Walls kept as individual `DrawModelEx` (~250 calls).
- Distance culling (16 tiles) as secondary optimization.
- Result: MAP_19 (6 enemies) = 300+ FPS, MAP_18 (42 enemies) = 246 FPS.

### Problem: FPS drops to ~84 in areas with many nearby enemies
- **Diagnosis**: Added perf logging to `debug.log`. Showed FPS correlated with enemy proximity, not position.
- **Fix**: Enemy AI culling (skip full AI for enemies >22 tiles away), sprite culling (skip rendering non-visible markers/enemies), chase BFS skip (only build when aggroed enemy is nearby).
- **Result**: MAP_18 steady at 246 FPS everywhere.

### Problem: `SetTargetFPS(60)` caused 99% CPU on Switch
- **Root cause**: raylib's frame limiter uses a busy-wait spin loop.
- **Fix**: Used `SetTargetFPS(0)` on Switch to rely on vsync. Later reverted to `SetTargetFPS(60)` on all platforms after performance was good enough.

### Final optimization: LOS-based rendering
- Replaced distance culling with `visible_tile_mask` — only render tiles the player can actually see.
- 256 rays × 96 steps per frame (covers full map).
- Infinite visual range — can see down long corridors.
- Result: Renders only what's visible, typically 100-200 tiles instead of 600+.

## 3. Controls

### Problem: Gamepad not detected on Switch
- `bt3d_active_gamepad_index()` returned -1 because `IsGamepadAvailable(0)` was false.
- **Fix**: On Switch, always return 0 (pad 0 is always the default controller).

### Problem: Left stick forward/backward didn't work
- **Root cause**: libnx `padGetStickPos` returns Y positive = up, but the game expected Y negative = up (standard gamepad convention). raylib-nx doesn't negate the Y axis.
- **Fix**: Negated Y axis for both sticks: `pad_move.y = -GetGamepadAxisMovement(...)`.

### Problem: D-pad didn't work in menus on startup
- **Initial diagnosis**: Assumed `IsGamepadButtonPressed` had a state tracking bug.
- **Attempted fix 1**: Manual edge detection with `IsGamepadButtonDown` + previous state tracking. Still didn't work.
- **Attempted fix 2**: Direct libnx `padGetButtons`/`padUpdate` reading, bypassing raylib entirely. D-pad showed as working in debug logs (`HidNpadButton_Up = 0x2000`), but right stick still navigated.
- **Root cause discovered via logging**: raylib-nx's `NX_SUPPORT_GAMEPAD_EMULATION` maps right stick digital thresholds to keyboard keys (`KEY_UP/DOWN`). The `MENU_KEY_UP()` macros read `IsKeyPressed(KEY_UP)`, so the right stick navigated through the keyboard emulation path.
- **Final fix**: On Switch, suppress ALL keyboard-based menu input (`MENU_KEY_*` returns 0, confirm/back skip `IsKeyPressed`). Only direct libnx `padGetButtons` handles d-pad/A/B/+. D-pad uses cooldown timer (0.2s) instead of edge detection.

### Problem: + button immediately closed menu after opening
- **Root cause**: Gameplay code sets `menu_active = 1` when + is pressed. On next frame, menu code reads + still held and interprets it as "back".
- **Fix**: Added `menu_notify_opened()` that sets a 0.15s cooldown. During cooldown, back input is ignored and `menu_back_held` is forced to 1.

## 4. Save System

### Problem: Save game crashed on Switch
- **Diagnosis**: Added step-by-step `debug_log` to `load_game_from_file`. Log showed load succeeded completely — header read, map loaded, state restored, `in_session=1`.
- **No FRAME log lines appeared** — crash happened after load but before the first render frame.
- Added render-path logging: `RENDER: tile_grid=... cube_loaded=1 floor_loaded=1 wall_loaded=0`.
- **Root cause**: `wall_plane_model_loaded = 0` — the wall plane model wasn't initialized because we removed the startup map preload. The model was previously created lazily inside `draw_wall_overlay_plane`, but after removing the preload, no map was loaded before the first render, so overlays tried to use an uninitialized model.
- **Fix**: Added `ensure_wall_plane_model(app)` to `load_initial_pack` alongside cube and floor plane models.

### Problem: Save directory didn't exist on Switch
- `sdmc:/switch/bt3d/` needs to be created manually.
- **Fix**: Added `mkdir(save_dir, 0755)` in `bt3d_init_window_and_audio` on Switch.

## 5. Rendering Issues

### Problem: Black rooms when opening doors
- **Root cause**: `visible_tile_mask` check on floor/ceiling rendering. When a door opens, tiles behind it aren't in the visibility mask yet (computed before door state changes).
- **Fix**: Changed door visibility blocking from `openness < 0.5f` to `openness <= 0.0f` — doors only block visibility when fully closed. Room behind renders immediately when door starts moving.

### Problem: Black spots on walls at visibility edges
- **Root cause**: Wall tile might be visible from its neighboring empty tile, but the wall tile itself isn't hit by any visibility ray (it's at the edge of the cone). Strict `visible_tile_mask` check skips it.
- **Fix**: After computing visibility mask, extend by 1 tile in all directions into `render_tile_mask`. Use `render_tile_mask` for rendering, keep original `visible_tile_mask` for gameplay (enemy aggro).

### Problem: Missing ceiling textures
- **Root cause**: Floor and ceiling quads used the same winding order. Floor faces up (correct from player height), ceiling also faces up (invisible from below).
- **Fix**: `flush_horiz_quads` now takes an `is_ceiling` parameter. Floor uses normal winding (visible from above), ceiling uses reversed winding (visible from below).

### Problem: Enemies aggroing through walls after visibility fix
- **Root cause**: `render_tile_mask` extension marked wall neighbors as "visible", and enemy aggro used `visible_tile_mask` which was being overwritten with the extended version.
- **Fix**: Separated into two masks — `visible_tile_mask` (original, for gameplay/aggro) and `render_tile_mask` (extended, for rendering only).

## 6. Audio

### Missing sounds identified by comparing with Go/browser port
- Door close (`SND_23`): Added to `bt3d_update_events` when `openness <= 0`.
- Ambient sounds: Added system matching Go port — pool of `SND_16,17,30-36,38-41`, plays every 45-60s with randomization, resets per map.

## 7. Tools and Debugging

### File-based logging
- `debug_log()` writes to `sdmc:/switch/bt3d/debug.log` on Switch.
- Used for: load-game step tracing, render state inspection, perf stats (FPS/entity counts per second).
- Key breakthrough: `wall_loaded=0` in render log revealed the save-load crash root cause.

### Docker build
- `build-switch-docker.sh` uses `devkitpro/devkita64` image for CI/portable builds.
- Patches raylib-nx inline (dlfcn.h stubs, raudio.c include order).

### Enemy count verification
- Used browser port's `parseMap242Entry` to count enemies per map from `data.pck`.
- Confirmed all counts match between original data and raylib port import filter.
- Later maps genuinely have 100-146 enemies — not a bug.

## 8. Desktop Visual Tuning

### Problem: Walls were tall rectangles, not squares
- **Initial state**: `WALL_HEIGHT 1.6f` against 1.0-wide cells made every wall face a 1:1.6 portrait rectangle.
- **Reference**: Browser port uses `PlaneGeometry(1, 1)` for wall faces and `CAMERA_HEIGHT 0.5`, i.e. the wall is a perfect cube with the eye at mid-height. Cross-checked the original via `data.pck` extracted patches and the screenshot collection.
- **Fix**: `WALL_HEIGHT 1.0f`, `PLAYER_HEIGHT 0.5f` (later 0.55 for a slightly higher horizon), all dependent constants (`MARKER_BILLBOARD_SIZE`, `ENEMY_BILLBOARD_SIZE`, door overlay sizes) re-derived against the new wall scale.

### Problem: FOV felt wider than the original
- **Initial state**: `camera.fovy = 58.0f`. At 16:9 that resolves to ~89 deg horizontal — way wider than a typical software raycaster (60-66 deg horizontal).
- **Iteration**: 40 deg too narrow, 50 deg ok, 55 still slightly tight, settled on 60 to balance "wider than raycaster" against modern widescreen aspect.

### Problem: Weapon viewmodel looked stretched/short
- **Root cause**: Original sprite art was authored for 320x200 VGA (1.2:1 vertical pixel aspect). Modern square pixels render the patch shorter than intended.
- **Fix**: Apply `0.9` X-scale and `1.15` Y-scale on top of the height-relative scale so the sprite reads at the original aspect.
- Settled on screen-height-relative base scale of `8.0` after iterating 6.4 (too small) -> 8.8 (too big) -> 8.0.

## 9. Door Frame / Overlay Investigation

### Symptom
Door frames (the wall-overlay trims that flank doors) and on-door VEC overlays
appeared inconsistently — some doors had them, most didn't, and even with debug
markers in place the overlay planes were either invisible or visible only when
the door slid open.

### What was actually wrong
1. The wall-overlay plane was inset to 0.9 of the wall and pushed out 0.03
   from the wall face. At shallow viewing angles the player saw the gap
   between the offset trim and the actual wall surface.
2. Pass 3 (wall overlays) gated on `render_tile_mask[wall_cell]`. Trim cells
   adjacent to a freshly-visible door cell had not yet entered the mask, so
   the overlay was withheld even though the door beside it was visible.
3. The on-door overlay plane was inset to 0.9 width / 0.9 height of the door
   and offset 0.10 from cell center. With the door cube only `0.18` thick,
   the offset placed the overlay 0.005 outside the door face — fine in
   isolation, but then `leaf_shift2` followed the door leaf as it slid
   open, so a closed door's overlay sat correctly against the cube and a
   moving door's overlay tracked it.

### Resolution (reached after several rounds)
- Wall overlays now render full-size (1.0 x WALL_HEIGHT) with `outward 0.015`.
- A new helper `pass3_overlay_face_borders_renderable_door` lets pass 3
  render an overlay even when the wall cell isn't itself in the render
  mask, as long as the face it points at borders an in-mask door cell.
- Door overlays render as 1.0 x WALL_HEIGHT (or 0.5 x WALL_HEIGHT for a
  half of a center-split door) at `outward = 0.18*0.5 + 0.005`, just
  outside the door surface, with `get_split_patch_texture` slicing the
  correct half of a split-leaf VEC sprite.

### Diagnostic loop
Confirmed step-by-step using two rounds of in-engine logging:
- Dumped `tile_grid` + `detail_grid[base..base+3]` for every door cell on
  load, plus 3x3 neighbour low-byte/face-bit info, to confirm `d1` is the
  on-door overlay id and the perimeter walls' low-byte is the trim id.
- Logged each pass-3 overlay decision (`PASS3 wall(x,y) face=N overlay=ID`)
  to confirm the render-mask gate was the source of the missing trims.
- Dropped 0.1 m red/green debug cubes at the overlay positions and a
  magenta-tinted full-scale plane to verify the geometry was being drawn
  at the right place but the texture was mostly transparent (and that the
  cube vs plane visibility diverged when the door cube occluded the
  texture).

## 10. Standalone Desktop Distribution

### Goal
Ship the desktop port without bundling the original `data.pck`.

### Pieces
1. `ChangeDirectory(GetApplicationDirectory())` at startup so the relative
   `data.pck` candidate resolves regardless of where the binary is launched
   from.
2. Trimmed `bt3d_get_data_pack_candidates` to a single user-facing path
   (`data.pck` next to the exe) plus the dev-tree fallbacks.
3. New full-screen "data.pck not found" frame is drawn whenever
   `pack.entry_count == 0` — shows the exact directory the player should
   populate, ESC quits.
4. `BT3D_STATIC_RAYLIB` CMake option links `libraylib.a` directly while
   still picking up the imported target's include dirs from `find_package`.
5. `BT3D_RAYLIB_ROOT` CMake option overrides `find_package` entirely with an
   explicit `{include,lib}` root. Required for the upstream
   `raylib-5.5_macos.tar.gz` release because it ships no CMake config.

### Universal macOS recipe
```
curl -sL https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_macos.tar.gz | tar -xz -C /tmp/
cmake -S . -B build-universal -DCMAKE_BUILD_TYPE=Release \
  -DBT3D_STATIC_RAYLIB=ON \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DBT3D_RAYLIB_ROOT=/tmp/raylib-5.5_macos
cmake --build build-universal
strip build-universal/bt3d_raylib
```
Result: 3.1 MB fat binary, links only macOS system frameworks
(IOKit / Cocoa / OpenGL / AppKit / CoreFoundation / CoreGraphics / Foundation).

### Strict pack lookup + native error dialog
- The first pass tolerated dev-tree fallbacks (`../data.pck`,
  `../../original/data.pck`, ...) so launching a release build from a
  copy of the project tree still found assets through the parent
  directories. That mismatched the documented player-facing rule
  ("drop data.pck next to the binary"). Now `bt3d_get_data_pack_candidates`
  returns exactly one path and the dev tree must mirror release behaviour.
- Replaced the in-game "data.pck not found" screen with a real OS error
  dialog (osascript on macOS, zenity / kdialog on Linux) fired right
  after `load_initial_pack` returns empty. The window is closed
  immediately after, so the player no longer sees an unresponsive game
  window.

## 11. Difficulty System

### Goal
Add Easy / Medium / Hard difficulty selection that changes enemy stats
without touching the per-class catalog used by AI/pathfinding/animation.

### Design
- Static `g_difficulty` in `enemies.c` plus four float[3] tables for HP,
  attack cooldown, initial reaction delay, and damage multipliers. The
  per-class stat functions multiply by the table entry for the current
  difficulty before returning, so call-sites in `main.c` keep the same
  signature.
- `AppState.difficulty` mirrors the value so it survives saves and
  difficulty selection happens through `bt3d_set_difficulty`.
- Default = Medium; `bt3d_reset_app_state` initialises both the field
  and the helper.

### Reaction reset on LOS loss
Player feedback: enemies reacting instantly the moment they re-spot the
player after the player ducks behind cover felt cheap. Two bad options
ruled out: completely de-aggroing on LOS loss (then enemies stop
chasing) and scaling the global cooldown (changes balance for combat
that never broke LOS).

Final solution: `EnemyRuntime` gained `had_los_last_tick`. When the LOS
flag transitions from false back to true, the initial reaction delay
(`bt3d_enemy_initial_attack_delay_for_class`, already difficulty-scaled)
is re-applied via `fmaxf` so the enemy re-acquires the player but waits
the same beat as the first sighting before attacking again. Aggro stays
true throughout so chase / pathfinding / door opening keep running.

### Save format v2
Adding `int difficulty` to `SaveHeader` was a layout change, so
`SAVE_FORMAT_VERSION` was bumped from 1 to 2 and old saves are
rejected by the existing magic/version check rather than silently
loading at the wrong difficulty. The load-menu label was extended to
`Level N [Easy/Medium/Hard] - YYYY-MM-DD HH:MM` so players can pick
the right slot for the run they were on.

### Menu flow
"New Game" no longer starts immediately; it routes through a new
`MENU_SCREEN_DIFFICULTY` panel (Easy / Medium / Hard / Back). The
selected option writes `app->difficulty` and runs `start_new_game`
in the same handler. Esc/B from the difficulty panel returns to the
main menu.

## 12. Wall corner seams

### Symptom
Outer corners where two perpendicular wall cells meet showed a thin
vertical strip of "behind the wall" whenever the player viewed the
corner diagonally. Walls used `draw_textured_wall_slab` with a 0.02
thick cube per face, and each slab terminated at its own cell edge,
leaving a 0.01 x 0.01 wedge at the junction that neither perpendicular
slab covered.

### First attempt (and why it failed)
A uniform `inset = -0.01` (each slab 1.02 world units long) closed
the corner gap but introduced a new problem: two collinear wall cells
now had their slabs extending 0.01 into each other, so the side faces
of the two 0.02-thick cubes z-fought on the shared seam and produced
a visible vertical tear along the whole length of a long wall.

### Fix
Two changes in `draw_textured_wall_slab`:

1. Replaced the symmetric `inset` with `start_ext` / `end_ext` so each
   end of a slab can extend independently. The caller asks the new
   helper `wall_cell_renders_face(cx, cy, face)` whether the
   length-axis neighbour renders the same face, and only extends
   toward "free" ends (neighbour is air, a door, map edge, or a wall
   that does not render that face). Collinear walls abut at exactly
   1.0 units so they no longer overlap.

2. Slab thickness dropped from 0.02 to 0.001 and the free-end extension
   from 0.01 to 0.001. The cube is now effectively a plane on screen;
   the side strip is below a pixel even at oblique angles, and the
   residual corner overshoot is still enough to close the pixel-scale
   gap that motivated the original bug report.

The cube geometry was kept (rather than switching to the
`wall_plane_model`) because an earlier plane-based rewrite landed in a
regression — texture orientation on some faces came out rotated and
the game looked visibly broken. The thin-cube approach preserves the
existing orientation.

## 13. Frame pacing

### Desktop FPS cap raised to 144
Original target `SetTargetFPS(60)` felt wrong on high-refresh displays
and left mouse look feeling stepped. Desktop now targets 144 fps under
`!BT3D_PLATFORM_SWITCH`.

### Switch uncapped
`SetTargetFPS(60)` was also dropped on Switch. Hardware vsync still
paces the present, so in practice the game runs at the display
refresh without a software cap interfering with raylib's frame
budget.

### Debug FPS counter
`bt3d_draw_real_hud` prints `FPS <n>` at the top-left when
`app->debug_tools_enabled` is on. Off by default so normal players do
not see a counter; the toggle lives in the Options menu.

## 14. Player respawn ammo

Death/respawn reloads the map and strips slots 3 and 4, but left
`player_weapon_ammo[2]` at whatever value it held on death — which
could be 0 if the player ran the pistol dry before dying, effectively
soft-locking the run. Added a single line to the non-restart branch
of the death-transition handler that resets the pistol ammo to 24,
the same value `init_player_state` uses.

## 15. Enemy projectile speed

`spawn_enemy_projectile` hardcoded `6.5f` on both velocity axes. The
player projectile speed is `PLAYER_PROJECTILE_SPEED` which comes out
to ~14.2, so enemy rounds were trivial to dodge at any medium range.
Raised to `10.0f` — still clearly slower than the player so dodging
is viable but no longer free.

## 16. VOS (class 5) projectile dealt no damage

Enemy projectile damage is applied by
`apply_enemy_projectile_explosion`. For non-blast projectiles
(`blast_radius <= 0.0f`) it only called `damage_player` if the player
was within 0.28 world units of the impact. But
`update_enemy_projectiles` triggers the explosion as soon as the
projectile is within 0.45 of the player, so every VOS hit sat in the
0.28..0.45 window and was discarded. Raised the non-blast damage
threshold from 0.28 to 0.5 so the proximity trigger and the damage
window agree.

## 17. Enemy hurt_timer aligned to pistol refire

Both hurt_timer sites (`damage_player` via splash and the direct-hit
branch in `apply_player_projectile_explosion`) used hand-tuned values
(0.4s and 0.18s). The splash value was longer than every weapon's
refire so sustained slot-4 fire stun-locked enemies forever; the
direct-hit value was shorter than every weapon so it did nothing.
Both now read from a new `PLAYER_PISTOL_REFIRE_SECONDS` constant
`(4 + 3) * GAME_TICK_SECS = 0.462s`, derived from slot 2's sequence
length. Pistol fire keeps an enemy in pain animation without over-
or under-shooting the window, and the constant gives a single place
to retune later.

## 18. Menu centering and save dialog layout

### Main menu vertical centering
`compute_menu_layout` pinned `panel_y` to
`fmaxf(260.0f, screen_h * 0.42f)`, which put the panel visibly below
the screen midpoint on 16:9 displays. Replaced with
`fmaxf(28.0f, (screen_h - panel_height) * 0.5f)` so the panel is
centred vertically with a small floor for short windows.

### Save Game dialog overlap
Before the fix:
- The "Name" label was drawn 24 px above the input box, which landed
  inside the "Save Game" title.
- Buttons were placed by adding `+36` to the row-based
  `layout.item_rects[i]`; for screens with smaller `row_height` this
  landed the Save button on top of the input field.
- `menu_screen_item_count` returned 2 for the save dialog, so the
  panel height was just enough for two rows and the Cancel button
  spilled outside the panel along with the footer hint.

Fix:
- `menu_screen_item_count` returns 4 for `MENU_SCREEN_SAVE_DIALOG` so
  the panel reserves room for title, label, input, two buttons, and
  the footer hint without overflow.
- The input rect moved to `panel_y + 80` so the "Name" label sits
  clearly between the title and the field.
- Save and Cancel buttons are positioned explicitly from
  `input_rect.y + input_rect.height + 18` with a fixed row spacing,
  rather than piggy-backing on `layout.item_rects` with a magic
  `y += 36`.

## 19. Split-door detection (keyed variants + f400 family)

`bt3d_is_center_split_door_tile` originally only recognised split-door
families `0xb000` and `0xb400`. The map data also uses:

- `0xb100` (one tile in MAP_30) and `0xb500` (13 tiles across several
  maps) — the same `b` families with the `0x0100` "keyed variant" bit
  set.
- `0xf400` / `0xf500` — a third split family discovered on
  MAP_18 `(38,27)`. Previously rendered as a single full leaf.

Fix: mask out the `0x0100` bit before the family compare, and accept
`0xf400` alongside the two `b` families.

```c
uint16_t family = tile & 0xfe00;
return family == 0xb000 || family == 0xb400 || family == 0xf400;
```

`0xb100 & 0xfe00 = 0xb000`, `0xb500 & 0xfe00 = 0xb400`, and
`0xf500 & 0xfe00 = 0xf400`, so all three keyed variants come in
automatically. Browser port carried the same limitation; documented
here so the fix can be mirrored there later.

## 20. Split-door slice-index mapping

### Symptom
On MAP_7 the door at cell `(39, 34)` rendered its two halves with the
wrong sides (slice 1 on the left leaf, slice 0 on the right). Tile
`0xb4c0` (family `0xb400`, low byte `0xc0`), axis `x-slide`,
`slices 1/0` in the debug HUD. Other split doors — same family, low
byte `0x40` — rendered correctly.

### Root cause
`bt3d_split_door_slice_index` (mirrored from browser) tried to pick
the slice by running a synthetic `rotation_y` through a sin/cos sign
check, adding `π` when the low byte was `0x40`. The intent was to
account for viewer-side flips, but the door renderer already applies
the same rotation when drawing the plane. The result was a double
correction: `0x40` doors cancelled out and looked right, `0xc0` doors
ended up swapped.

### Fix
Dropped the sin/cos path. First attempt was a single sign check on
`leaf_offset` (every door maps negative→0, positive→1), which fixed
MAP_7 `(39,34)` but broke `0x40` z-slide doors in MAP_3, MAP_6,
MAP_17, and MAP_18 — they need the slices mirrored.

Final rule:

```c
int bt3d_split_door_slice_index(uint16_t tile, int slides_on_x_axis, float leaf_offset) {
    int swap = ((tile & 0xff) == 0x40) && !slides_on_x_axis;
    if (swap) return leaf_offset < 0.0f ? 1 : 0;
    return leaf_offset < 0.0f ? 0 : 1;
}
```

The only viewer-side flip we need is for doors whose low byte is
`0x40` AND whose leaves slide along Z (so the door's "front" faces
+X). For every other combination (`0x40` x-slide, `0xc0` either axis)
the natural `leaf_offset < 0 → slice 0` mapping applies.
Validated against sample doors in MAP_3 `(30,52)`, MAP_6 `(28,26)`,
MAP_7 `(39,34)`, MAP_17 `(31,4)`, and MAP_18 `(19,58)`.

### Debug helper
While chasing this, `C` was extended to prefer
`copy_looked_at_door_text` over the existing `copy_player_coordinates`
when the player is aimed at a door, falling back to markers and then
player coords. Makes "give me the cell" debugging much less painful —
look at the door, press `C`, paste the line.

Browser port's `splitDoorSliceIndex` has the same over-engineered
sin/cos logic and would benefit from the same collapse.

## 21. Combat refactor regressions

The combat/projectile extraction into `src/combat.c` inadvertently
changed three player-facing tables:

- Weapon fire sound IDs went from 5 / 1 / 2 / 3 (fist / pistol / DD7 /
  plasma) to 6 / 8 / 10 / 11. The data pack only ships `SND_1`,
  `SND_2`, `SND_3`, and `SND_5`, so every player weapon fell silent
  — most visibly the pistol. Restored the original IDs.
- `player_weapon_behavior` damage numbers were inflated (fist 3 -> 8,
  pistol 6 -> 18, slot 3 10 -> 38, slot 4 30 -> 60), and attack
  ranges were changed from `INFINITY` to finite numbers. With the
  new damage, even Easy HP scaling (x0.6) did not matter — enemies
  died in one or two shots everywhere, so "difficulties felt
  broken". Restored the original damage / range tables.
- Sequence lengths used for `player_weapon_refire_seconds` /
  `player_weapon_attack_timer_seconds` were also shifted in
  `combat.c`, while `render_ui.c` still used the old 3 / 4 / 5 / 6
  values. That left the viewmodel animation length out of sync with
  the actual cooldown. Reverted to the original 3 / 4 / 5 / 6 in
  combat.c so both files agree.

The slot 2 pistol case keeps `PLAYER_PISTOL_REFIRE_SECONDS`, so the
hurt_timer / refire alignment from section 17 still holds.

## 22. Level 2 stutter (ceiling-marker O(N×cells) scan)

`sample $PID 3` against the game while level 2 was stuttering showed
`cell_has_ceiling_marker` dominating the frame:

    Sort by top of stack, same collapsed (when >= 5):
        ...
        cell_has_ceiling_marker  (in bt3d_raylib)        246
        ...
        bt3d_draw_map_world      (in bt3d_raylib)        6

The function did a linear scan of `app->markers` for every call, and
`bt3d_draw_map_world` / the visibility pass each call it once per
visible cell. On maps with many markers (MAP_18 has a lot) this was
`O(marker_count * visible_cells)` per frame.

Fix: add `unsigned char ceiling_marker_cell_mask[BT3D_GRID_CELLS]` to
`AppState`, populate it in `build_runtime_state` when the map is
loaded (each marker that passes `marker_hangs_from_ceiling` sets its
cell bit using the same `base.y / base.x` swap the renderer uses),
and clear the bit on pickup. `cell_has_ceiling_marker` becomes an
`O(1)` mask read:

```c
int cell_has_ceiling_marker(AppState *app, int cell_x, int cell_y) {
    if (!app) return 0;
    if (cell_x < 0 || cell_y < 0 || cell_x >= BT3D_MAP_WIDTH || cell_y >= BT3D_MAP_HEIGHT) return 0;
    return app->ceiling_marker_cell_mask[cell_x * BT3D_MAP_WIDTH + cell_y];
}
```

Also updated the pickup path in `main.c` to clear the bit when the
marker is collected so subsequent frames don't keep treating the
cell as ceiling-occluded.

## 23. Wall corner dark line (take 3)

Sections 12 and the earlier zero-thickness rewrite both tried to kill
the visible outer-corner gap. The thin cube + small extension pair
(thickness 0.001, overshoot 0.001) closed the gap but left a faint
dark line along the shared edge where two perpendicular wall faces
met. Comparison against the browser port confirmed the browser had
no such line.

Root cause: the small per-end extension applied to both perpendicular
slabs caused them to overlap by ~0.0005 world units at the corner.
Two co-planar textured faces at near-identical depth z-fought there
each frame, which on macOS Metal rendered as a 1 px dark seam.

The browser port places its wall planes at the exact cell boundaries
(`posX`/`posZ` are 0, 1, or 0.5 with no overshoot). Mirrored the same
approach here: set thickness to 0 (degenerate cube = single plane
per face) AND drop the `start_ext` / `end_ext` extensions to 0. The
`wall_cell_renders_face` helper is no longer needed and was removed.

With both planes landing exactly on `cell_x / cell_x+1 / cell_y /
cell_y+1` boundaries, perpendicular faces share only an infinitely
thin vertical edge — no overlap, no z-fight, and the corner reads
clean. Collinear walls along the same face also abut without
overlapping, so no regression there either.

## 24. Dark line on floor under wall overlays (depth-write on discard)

Wherever a wall carried an overlay (decorative vase, door frame
trim, vent, etc.) a thin black line ran across the floor at the
wall's base. It was visible only under overlays; plain wall cells
with no overlay did not show it. Disabling the `draw_wall_overlay_plane`
call confirmed the overlay was the sole trigger.

### Why it happened
`wall_plane_model` uses a custom fragment shader with alphaTest:

```glsl
vec4 c = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
if (c.a < 0.5) discard;
finalColor = c;
```

On modern GPUs with early-Z (macOS Metal in particular), the hardware
commits the pixel's depth value **before** the fragment shader runs.
When the shader then discards, the color write is cancelled but the
depth write was already committed.

The overlay plane spans y=0..WALL_HEIGHT. Its bottom row intersects
the floor plane at y=0. VEC sprites tend to have transparent pixels
at the bottom, so the shader discards them — but the depth value at
that row had already been written. The stored depth matched the
floor plane at the same line, and the GPU then failed subsequent
floor/overlay depth tests at those pixels, leaving the uncleared /
cleared background colour showing through as a dark line.

### Fix
`rlDisableDepthMask()` before the overlay `DrawModelEx` and
`rlEnableDepthMask()` after. Depth **testing** still runs (overlays
don't render through walls), but depth **writes** are suppressed so
the overlay can never corrupt the depth buffer at its transparent
rows. The floor plane's depth stays authoritative and the dark line
is gone.

This matches the pattern raylib itself uses in `DrawBillboard` for
alphaTest sprites.

## 25. Door frame / hinge floating off the wall

### Symptom
The decorative "door frame" art on walls next to a door visibly
floated in mid-air — you could sidestep up to a door and see clear
space between the frame art and the wall it was supposedly painted
onto. The door overlay (hinge strips on the door face) and the wall
overlay (frame art on adjacent wall cells) both sat too far out from
their surfaces.

### Cause
Two decorative planes had wide outward offsets set originally as
crude z-fight guards:

- `draw_wall_overlay_plane` pushed its plane 15 mm off the wall face
  (`outward = 0.015f`).
- `draw_door_overlays` pushed the overlay plane
  `0.18 * 0.5 + 0.015 = 0.105` units off the door centre — 15 mm past
  the door cube's face.

Section 24's `rlDisableDepthMask()` fix already removes the z-fight
risk for the overlay planes, so the large outward offsets are no
longer needed.

### Fix
- Wall overlay outward trimmed from `0.015f` to `0.001f` (1 mm,
  essentially flush against the wall).
- Door overlay offset trimmed from `0.18 * 0.5 + 0.015` to
  `0.18 * 0.5 + 0.001` (1 mm past the door face).

Both decorations now read as painted onto their surfaces when viewed
from the side.

## 26. Render culling flicker at distance

### Symptom
While walking and turning, cells in the mid-to-far field visibly
toggled between their textured wall and a black void, or between
"full brightness" and "dim" — especially noticeable near doorways
and at the edges of long corridors.

### Cause
`bt3d_refresh_visible_tile_mask` builds the per-frame visibility set
by firing `VISIBILITY_MASK_RAY_COUNT` rays at evenly spaced angles
around the player and marking each traversed cell. Renderer then
extends the mask by one tile (`render_tile_mask`).

The old count was `256`, which gives `2π / 256 ≈ 1.4°` between rays.
At `~20` tiles distance two adjacent rays diverge by
`20 * tan(1.4°) ≈ 0.49` tiles — wider than a cell. Cells that fall
between rays are left unmarked and the renderer skips them that
frame, showing the clear colour through the gap. As the player
rotates, the gap sweeps over different cells -> flicker.

### Fix
Raised `VISIBILITY_MASK_RAY_COUNT` from `256` to `1024` in
`src/bt3d.h`. Angular resolution is now `~0.35°`, so adjacent rays
stay under a cell apart out to the max ray step count. Cost is 4x
the DDA stepping per frame (still sub-millisecond) and the flicker is
gone.

## 27. Switch 60fps push — in-engine profiler + render CPU cuts

After the `1024`-ray visibility (§26), split doors (§19-§20), and
other per-frame geometry work piled up, Switch frame time on dense
rooms landed around `22.5ms / 43fps`. Target: lock 60fps. Plan in
`~/.claude/plans/i-d-like-to-better-eager-stonebraker.md`; this
section records what shipped.

### Tool: in-engine profiler logging over nxlink
`src/profiler.h` / `src/profiler.c`. Zone-stack timer using raylib
`GetTime()`. API is three macros:
```
BT3D_PROF_BEGIN("name");
... work ...
BT3D_PROF_END("name");
```
and a toggle (`bt3d_profiler_toggle()`). Toggle: `F10` on desktop,
`Minus` on Switch (requires debug tools enabled — set via the options
menu).

When enabled and a frame exceeds `BT3D_PROFILER_SLOW_MS` (17 ms,
i.e. below 60fps), the profiler prints one sorted-desc line to
`stdout`:
```
[prof] frame #1823  21.34ms  fps 47  visibility 12.40 draw_world 5.12 ...
```
On Switch, `nxlinkStdio()` (`src/main.c:1266`) is already wired, so
`printf` streams to the host `nxlink` listener:
```
docker run --rm -p 28771:28771 -v /Users/user/bt3d-switch:/mnt \
  devkitpro/devkita64:20260215 \
  nxlink --address <SWITCH_IP> -s \
  -p /switch/bt3d/bt3d.nro \
  /mnt/raylib-port/build/switch/bt3d_raylib_nx.nro
```
Rate-limited to one log per 100ms so the stream stays readable
through sustained slowdowns. Module is zero-cost when disabled
(`g_enabled` short-circuits every hook).

**Caveat (host build):** on Switch `GetFPS()` returned bogus values
(`1805`, `1793`) — useless; trust the `ms` field. Frame `#1` also
had a junk timestamp (`GetTime()` not warm), easy to ignore.

### Phase 2a (kept, small win): visible-cell list
`src/bt3d.h` now stores `visible_cell_indices[BT3D_GRID_CELLS]` +
`visible_cell_count`, populated at the end of
`bt3d_refresh_visible_tile_mask` (`src/visibility.c`) from the final
`render_tile_mask`. Also `overlay_cell_indices` built once per map
load in `session.c:load_map_entry` (solid cells are static).

`render_world.c` render loops walk these lists instead of the full
64×64 grid. Expected: O(visible) instead of O(4096). Measured: **no
win in dense rooms** where ~3000 of 4096 cells are visible after
dilation — the skip-via-mask branch was basically free, and the
visible count ≈ grid count. Kept the change anyway because it's
correct, costs nothing, and helps sparser rooms.

### Phase 2b (big win): O(N×events) → O(1) + static cell caches
Profiler showed `dw_overlays` at 4.5ms and `update_player` at 2.5ms,
both dominated by `bt3d_find_event_by_cell` — a linear scan of the
events array called 3-4× per visible cell per pass.

Added `short event_index_by_cell[BT3D_GRID_CELLS]` to `AppState`
(`src/bt3d.h`), populated in `build_runtime_state` (`src/session.c`)
whenever events are (re)initialized. Rewrote `bt3d_find_event_by_cell`
(`src/doors.c`) as an O(1) cell-index lookup into that array.

Also added static caches for per-cell queries that don't change
after map load:
- `unsigned char structural_wall_mask[BT3D_GRID_CELLS]`
- `unsigned char raw_floor_tex_cache[BT3D_GRID_CELLS]`
- `unsigned char raw_ceil_tex_cache[BT3D_GRID_CELLS]`

All three populated by `bt3d_build_static_cell_caches` in `main.c`,
called from `session.c:load_map_entry` after `build_runtime_state`
(ordering matters — depends on `ceiling_marker_cell_mask` already
being built).

`render_world.c` wall loop now tests `app->structural_wall_mask[idx]`
directly instead of `is_structural_wall_tile(...)`. The horiz-gather
loop reads the raw-tex caches for non-event cells and only falls back
to `floor_texture_index_for_cell` when the cell has an active event
(rare).

Measured:
- `dw_overlays` 4.5ms → 2.1ms
- `update_player` 2.5ms → 1.15ms (visibility internal calls halved)
- `dw_horiz_gather` 5.8ms → 5.5ms (still dominated by the quad build
  at this point — fixed in Phase 2c's cache wiring)
- Frame total ~22.5ms → ~18ms

Not 60fps yet. `dw_walls_and_doors` (8ms) unchanged — per-face
`DrawModelEx` still the bottleneck.

### Phase 2c (unlocks 60fps): batch wall quads via `rlBegin/RL_QUADS`
The baseline wall path called `draw_textured_wall_slab` per face →
one `DrawModelEx(cube_model, ...)` with `scale.z = 0` per face →
~2000 draw calls/frame in dense rooms. GL state churn + matrix
upload per call was the 8ms.

Same trick `flush_horiz_quads` already uses for floors/ceilings:
collect quads into a CPU buffer, sort by texture, emit via
`rlBegin(RL_QUADS)` one batch per texture. New in `render_world.c`:

- `WallQuadEntry { tex_id, cell_x, cell_y, face }` — 8 bytes.
- `g_wall_quads[BT3D_GRID_CELLS * 4]` static buffer (128 KB).
- `flush_wall_quads(app)` — qsort by `tex_id`, iterate emitting 4
  texcoord+vertex pairs per face, grouped by texture between
  `rlBegin`/`rlEnd`. Wraps the loop in
  `rlDisableBackfaceCulling()` / `rlEnableBackfaceCulling()`, same
  as `flush_horiz_quads`.

The structural-wall branch of the walls_and_doors loop now just
pushes a `WallQuadEntry` instead of calling
`draw_textured_wall_slab`. Doors keep the DrawModelEx path (dynamic
animation, few per frame).

### Bug: initial winding made "a lot of the walls completely missing"
First cut had CCW winding with normals pointing **into** the wall
cell. Despite `rlDisableBackfaceCulling`, most walls didn't render
from the usual viewing angle (player standing in the adjacent
non-wall cell).

Hypothesis: the rl batch renderer on raylib-nx GLES2 flushes
vertices with `glDrawElements` at a GL state captured somewhere that
wasn't the immediate `glDisable(GL_CULL_FACE)` call. Didn't dig
further — fix was to reverse the winding so the front face is
outward (the side the player actually looks at), which is the right
thing to do regardless of cull state.

Each of the four wall faces now emits in this order, UVs unchanged:
```
(1,1) bottom-far    (0,1) bottom-near    (0,0) top-near    (1,0) top-far
```
where "near"/"far" flip per face so the outward-facing normal is
`-X / +X / +Z / -Z` for faces 0/1/2/3. See `flush_wall_quads` in
`src/render_world.c` for the exact verts.

After the fix, all walls render and the draw is pixel-identical to
the baseline cube-model path.

### Final numbers (dense F400-ish room, Switch, profiler on)
| zone                | before 2a | 2b    | 2c    |
|---------------------|----------:|------:|------:|
| `draw_world`        | 18.7ms    | 15.9  |  3.8  |
| `dw_walls_and_doors`|  8.2ms    |  8.0  |  2.4  |
| `dw_horiz_gather`   |  5.8ms    |  5.5  |  0.15 |
| `dw_overlays`       |  4.5ms    |  2.1  |  1.1  |
| `update_player`     |  2.5ms    |  1.15 |  1.0  |
| frame total         | 22.5ms    | 18.3  | 16.9  |
| steady fps          | 43        | 53    | **60**|

The `dw_horiz_gather` collapse in 2c came from the Phase 2b raw-tex
caches finally being wired into the quad-build loop (reads
`raw_floor_tex_cache[idx]` for non-event cells instead of
`floor_texture_index_for_cell` which traverses detail_grid + runs
`first_valid_texture_code`).

### What's NOT done
- Full VBO bake of static geometry — not needed, immediate-mode
  quad stream via `rlBegin/RL_QUADS` is fast enough on Switch.
- Wall texture atlas — skipped; per-texture batches are <~40, cheap.
- BFS visibility to replace 1024-ray DDA — skipped; `visibility` is
  ~1ms at 60fps, below the pain threshold.
- Billboard sort trim — 0.25ms, not worth touching.

### Files touched
- `src/profiler.h` / `src/profiler.c` — new module
- `src/main.c` — `BT3D_PROF_BEGIN/END` zones around hot loop; F10 /
  Minus toggle; `bt3d_build_static_cell_caches` implementation
- `src/bt3d.h` — `visible_cell_indices`, `overlay_cell_indices`,
  `event_index_by_cell`, `structural_wall_mask`,
  `raw_floor_tex_cache`, `raw_ceil_tex_cache` fields
- `src/bt3d_internal.h` — `#include "profiler.h"`; declare
  `bt3d_build_static_cell_caches`
- `src/visibility.c` — emit `visible_cell_indices` at end of
  `bt3d_refresh_visible_tile_mask`
- `src/session.c` — populate `event_index_by_cell` in
  `build_runtime_state`; emit `overlay_cell_indices` and call
  `bt3d_build_static_cell_caches` from `load_map_entry`
- `src/doors.c` — O(1) `bt3d_find_event_by_cell`
- `src/render_world.c` — wall batching (`WallQuadEntry`,
  `g_wall_quads`, `flush_wall_quads`), visible-list iteration in
  the three render passes, sub-zone instrumentation
- `CMakeLists.txt` — add `src/profiler.c`

## 28. Switch profiler follow-up — billboard/asset-load spikes

After the wall batching pass, another `nxlink` profiler run showed
steady-state geometry was no longer the obvious bottleneck. The first
samples from dense gameplay showed:

- `draw_billboards` often at `2.5ms` to `3.6ms`.
- `dw_overlays` steady around `~1.0ms`.
- `dw_walls_and_doors` usually around `1.5ms` to `1.8ms`, with
  occasional door/texture-load spikes.
- `visibility` stable under `~1ms`.
- one-off `update_player`, `events`, `draw_hud`, and `dw_floors`
  spikes lined up with asset loading in the `nxlink` log.

The important clue was not the aggregate billboard timing by itself:
the stream showed `TEXTURE loaded successfully` during active play.
That meant the slow frames were often paying patch decode/upload cost,
not pure sorting or draw work.

### Instrumentation added

`bt3d_draw_billboard_sprites` now has nested profiler zones:

- `db_markers`
- `db_enemies`
- `db_enemy_proj`
- `db_sort`
- `db_draw`

Enemy projectiles are also culled against `render_tile_mask` before
sprite name generation, texture lookup, sort, and draw. That avoids
paying billboard work for projectiles outside the visible render set.

### Patch prewarming

Added map-load patch prewarming in `src/session.c` after
`bt3d_build_static_cell_caches`:

- marker `VEC_*` patches used by the map
- enemy family frame patches for enemy classes present on the map
- projectile family frame patches for projectile-capable enemies
- wall-overlay mirrored patch variants
- door-overlay split patch variants
- all frames for runtime animated overlay remaps, not just the current
  frame returned by `animated_overlay_id`

To support that without duplicating frame tables, `main.c` exposes
`bt3d_runtime_vec_remap_frames`, and `enemies.c` exposes
`bt3d_sprite_frame_count_for_class`.

### Patch cache capacity

The first prewarm attempt still produced late `db_markers` spikes.
Cause: `patch_textures[256]` was too small once normal, mirrored,
split, enemy, projectile, marker, and overlay variants were all
preloaded. The cache filled, then later lookups missed and decoded
fresh textures during gameplay.

`MAX_PATCH_TEXTURES` is now `768`, and all patch-cache loops in
`assets.c` use that limit instead of hardcoded `256`.

### Measured outcome and remaining problem

After prewarming and increasing cache capacity, the common case for
`draw_billboards` dropped back to a small cost:

- typical `draw_billboards`: `0.06ms` to `0.17ms`
- typical `db_markers`: `0.07ms` to `0.12ms`

However, the final Switch run still showed active gameplay
`TEXTURE loaded successfully` lines and occasional marker spikes:

```
draw_billboards 4.02  db_markers 3.97
draw_billboards 1.71  db_markers 1.65
```

Those were followed by more stable frames such as:

```
draw_billboards 0.17  db_markers 0.12
draw_billboards 0.10  db_markers 0.07
```

So this pass improved the steady-state path and narrowed the issue,
but did not fully remove lazy patch loading. The next optimization
should broaden preload coverage by auditing the actual `VEC_*` ids
that still load during gameplay. The likely candidates are marker or
UI/special-prop patch ids reached through code paths not covered by
the current marker/enemy/overlay prewarm sets.

### Other remaining spikes

The profiler also showed non-billboard spikes:

- `update_player` / `events` spikes around lazy `WAVE` loads.
- `draw_hud` spikes around late bitmap loads.
- intermittent `dw_floors` / `dw_walls_and_doors` spikes, often near
  asset loading or transitions rather than steady-state traversal.

Next broad asset pass should include common `SND_*` and HUD/menu
`BM_*` preloads in addition to filling the remaining `VEC_*` gaps.

### Files touched

- `src/render_world.c` — billboard sub-zones and enemy projectile
  render-mask cull.
- `src/session.c` — map-level patch prewarm.
- `src/assets.c` / `src/bt3d.h` — `MAX_PATCH_TEXTURES = 768`.
- `src/enemies.c` / `src/bt3d.h` — sprite frame-count helper.
- `src/main.c` / `src/bt3d_internal.h` — runtime overlay remap frame
  helper for animated prewarm.

## 29. Switch profiler follow-up — targeted asset prewarm tradeoff

The next profiler run confirmed that the main renderer was no longer
the dominant steady-state problem. Slow frames now correlated with
raylib asset-load log lines during gameplay:

- `TEXTURE loaded successfully` near `draw_billboards`, `dw_floors`,
  `dw_walls_and_doors`, or `dw_overlays` spikes.
- `WAVE loaded successfully` near `update_player` / `events` spikes.
- HUD bitmap loads near `draw_hud` spikes.

This pass moved the common lazy loads to `load_map_entry`, after static
cell caches are built:

- normal `VEC_*` patches from the pack, plus marker base patches;
- wall and door `STN_*` textures used by the loaded map, with
  split-door slice variants for doors present in that map;
- common HUD/map `BM_*` and `M_*` bitmap textures;
- common gameplay sounds: UI, doors, pickups, ambient, weapons, and
  enemy aggro/attack/death ids.

`main.c` exposes `bt3d_prewarm_sound_id` so session loading can populate
the sound cache without playing anything.

### Tradeoff found

A broader experiment also generated every mirrored and split `VEC_*`
variant during level load. That removed more possible runtime texture
generation but made entering a level noticeably slower, because hundreds
of texture uploads and `LoadImageFromTexture` conversions were front-
loaded before the first frame.

That broad variant pass was rolled back. The better balance is targeted
prewarming: remove the common gameplay hitches, but avoid generating
every possible transformed asset unless the profiler proves a specific
variant repeatedly causes visible stutter.

### Files touched

- `src/session.c` — targeted texture/bitmap/sound prewarm from
  `load_map_entry`.
- `src/main.c` / `src/bt3d_internal.h` — `bt3d_prewarm_sound_id`.

## 30. Wall batch follow-up — overlay depth ordering

The wall/floor/ceiling batching pass changed two old renderer assumptions:

- batched wall UVs and horizontal UVs must match the cube-model orientation,
  or floor, ceiling, and wall textures appear flipped/rotated;
- alpha-tested wall overlays are very close to their backing wall, so drawing
  backing walls later through the sorted wall batch can depth-write over the
  overlay plane and make patches/hinges disappear.

The fix keeps the fast path for ordinary structural walls, but routes exposed
wall faces that carry an overlay through the original immediate
`draw_textured_wall_slab` path. Door-adjacent faces use the same exception
because door hinge overlays can otherwise be hidden when the door animation
changes the nearby draw/depth order.

This is intentionally a targeted performance tradeoff: only overlay-bearing
faces and door-adjacent overlay faces leave the batch. The broad structural
wall batch remains active for the common case.

### Switch profiler check

An nxlink profiler run after the fix showed normal steady-state slow-frame
samples still dominated by `draw_world`, with `dw_walls_and_doors` around
1.6-2.7 ms and `dw_overlays` usually under 0.9 ms. A single 19.7 ms sample
reported `dw_overlays` at 4.29 ms, but it coincided with a `TEXTURE loaded
successfully` log line, so that spike appears to be lazy asset upload rather
than the wall-batch bypass itself.

### Files touched

- `src/render_world.c` — restored batched UV orientation; bypasses the sorted
  wall batch for overlay-bearing exposed wall faces and door-adjacent faces.

## 31. Hidden-door triggers — class 7 descriptor tiles

Some hidden-door activations are carried by class `7` descriptor tiles:
the tile low byte selects a descriptor-table entry, and descriptor kind `1`
opens the linked remote event.

The port already scanned both class `6` and class `7`, but then filtered
with `is_special_prop_tile(tile)`. That helper intentionally includes the
whole class `7` family (`0x1800` mask), so class `7` triggers were found
and then immediately skipped.

Removing that extra special-prop skip lets both class `6` and class `7`
descriptor triggers activate their linked hidden doors. Ordinary special-prop
rendering/visibility rules are unchanged; this only affects the remote
descriptor activation path.

### Files touched

- `src/main.c` — allow class `7` remote descriptor triggers to reach the
  descriptor lookup/open-event logic.

## 32. Enemy stun behavior — hold pain frame and stop movement

Enemy pain animations use a single authored frame. The old animation advance
path left pain after one tick, but `hurt_timer` was still active, so the AI
kept re-entering pain and the enemy visually flickered between pain and walk.

Pain now holds its frame until `hurt_timer` expires. While stunned, enemies
also skip chase and attack logic, so taking damage briefly stops movement for
the same stun window. Aggro/LOS bookkeeping and timers still update.

### Files touched

- `src/enemies.c` — pain animation no longer auto-returns to walk.
- `src/enemy_ai.c` — active `hurt_timer` short-circuits movement/attack and
  keeps the enemy in pain animation.

## 33. Player/enemy contact, death-sprite cleanup, and HUD backing

Several small gameplay/rendering polish fixes shipped together:

- Desktop mouse buttons now use an explicit edge detector. This keeps left
  mouse fire and right mouse interact as single press events even on
  backends where `IsMouseButtonPressed` can behave like a held state.
- Player movement now checks living enemies after wall collision. If a
  candidate X/Z move would push the player deeper into an enemy collision
  radius, that axis is rejected. Moving away from an already-overlapping
  enemy remains allowed, and noclip still bypasses the check.
- Patch texture loading records both the raw top opaque row and a
  `visual_min_opaque_y` row with enough local support to count as real
  sprite body. Dead-enemy billboards crop from this visual top and stay
  bottom-anchored, which hides isolated stray pixels in some death frames
  that could appear as a dark/brown line above the corpse.
- Billboard draw entries now carry a source rectangle and independent
  width/height, and rendering uses `DrawBillboardPro`. Normal markers,
  living enemies, and projectiles still draw their full patch; only dead
  enemy frames use the cropped source.
- The centered HUD no longer draws a full-screen-width dark backing strip
  behind `BM_LISTA`, so the old side continuation disappears on wide
  desktop screens.

### Files touched

- `src/main.c` — mouse edge detector and player/enemy movement blocking.
- `src/assets.c` / `src/bt3d.h` — patch visual-top metadata and richer
  billboard draw entries.
- `src/render_world.c` — cropped dead-enemy billboard path via
  `DrawBillboardPro`.
- `src/render_ui.c` — removed the full-width dark HUD backing rectangle.

## 34. Player hitscan — first impact wins

The old raylib hitscan path selected the frontmost enemy within a loose
screen-center band and then used `bt3d_has_line_of_sight` to the enemy
center. That was close, but it was not the original game's shape: a shot
should collide along the firing direction, with walls/closed doors able to
win before an enemy.

IDA reference on the original 16-bit executable:

- `BT3D_HandleActorAttackOrSpawnProjectile` (`0x8123`) is the actor
  attack/spawn helper.
- Its direct-fire branches call `BT3D_TraceActorShotOrMovement`
  (`0x95de`) before applying damage.
- The trace helper returns either a hit actor index or impact coordinates.
  The miss path then spawns a transient impact/miss visual at those traced
  coordinates.

The raylib port now mirrors that first-impact rule for player direct-fire
weapons. `find_player_weapon_target` first DDA-traces the shot direction
from the player to find the nearest blocking wall/closed-door distance.
Living enemies are then intersected as circles against that same ray, and
only the closest enemy intersection before the wall is damaged. Infinite
range weapons use a practical 96-tile trace cap, which is beyond the 64x64
map diagonal.

Shots now spawn a short-lived impact billboard at the traced collision
point, whether that point is a wall/closed-door miss or the ray/enemy
intersection for a hit. The effect uses the original PSK sprite family
(`bt3d_sprite_entry_name_for_class(9, frame)`), matching the IDA-observed
original transient object path without turning direct-fire weapons into
moving projectiles.

### IDA labels added

- `BT3D_HandleActorAttackOrSpawnProjectile` at `0x8123`.
- `BT3D_TraceActorShotOrMovement` at `0x95de`.
- `BT3D_UpdateActiveWeaponScriptAndFire` at `0x7c09`.
- `BT3D_AdvanceActiveWeaponScriptFrame` at `0x7cce`.

### Files touched

- `src/combat.c` — added DDA wall distance tracing, ray/circle enemy
  intersection, and transient shot-impact lifetime updates.
- `src/bt3d.h` / `src/bt3d_internal.h` — transient shot-impact runtime
  storage and update declaration.
- `src/main.c` / `src/savegame.c` — clear transient shot-impact state on
  new games and loaded saves.
- `src/render_world.c` — draw PSK impact billboards with the existing
  sorted sprite pass.

## 35. Level transition screen background

The raylib transition screen had drifted from the original: it drew the
current gameplay frame with a translucent black overlay, then placed
`BM_LEVEL` and the level digits on top. The original executable does not
use the live gameplay frame as the transition background.

IDA reference on the original 16-bit executable:

- `BT3D_LoadLevelTransitionScreenAssets` (`0x3bd5`) first loads `bm_pod`.
- The same setup then loads `bm_level`.
- Two-digit level numbers load a tens digit bitmap, and every level loads
  the ones digit bitmap from the `bm_n0`-`bm_n9` table.
- `BT3D_CenterLevelTransitionPanel` (`0x3d74`) centers the transition UI
  panel before the one-shot timer advances the inter-level sequence.

The port now draws level transitions as their own screen. It skips the
gameplay world/HUD render while `transition_active` is set, draws `BM_POD`
as a tiled full-screen background, then draws the existing `BM_LEVEL` label
and numeric bitmap layer over it. This restores the authored inter-level
screen while keeping the transition state/timing unchanged.

### IDA labels added

- `BT3D_CenterLevelTransitionPanel` at `0x3d74`.
- `BT3D_SetUiTimerDelayTicks` at `0x11eb2`.

### Files touched

- `src/main.c` — skip gameplay world/HUD rendering during the level
  transition screen.
- `src/render_ui.c` — draw the original `BM_POD` transition background
  instead of dimming the gameplay frame.

## 36. Main menu art and music

The raylib menu had been a plain dark background with a modern panel. The
original executable uses packed bitmap art for the title/menu flow:

- `BT3D_13` (`0x3f0`) loads `bm_bgr_a` during early startup.
- `BT3D_ShowTitleMenuBitmapAndAdvance` (`0x811`) loads `bm_bt` into the
  child bitmap panel for the title/menu path, then unloads it before
  advancing after input.

After testing, we decided not to use `BM_BT` behind the interactive raylib
menu because it fought the current selectable menu layout. The port now
uses the same original blue `BM_POD` background as the level-transition
screen before drawing the selectable menu rows.

The original menu music is `original/m1.dat`, a Standard MIDI file outside
`data.pck`. Raylib 5.5 does not load MIDI directly; its music stream loader
supports WAV/OGG/MP3/FLAC/QOA/XM/MOD, but not `.mid`. To avoid shipping a
rendered audio copy or a large runtime SoundFont, CMake embeds `m1.dat` into
the executable at build time and the port parses it with TinyMidiLoader. A
small built-in synth renders the few instruments and drum sounds the menu
track needs into a raylib `AudioStream`. This keeps the game behavior
independent of the host operating system's MIDI stack and gives Windows,
macOS, Switch, and future ports the same playback path.

`BT3D_MIDI_SRC` points at the build-time source MIDI. The runtime no longer
looks for `m1.dat`, `soundfont.sf2`, or any other loose music asset beside the
executable.

`bt3d_update_menu_music` starts MIDI only on the startup main-menu screen
and stops it when gameplay starts, when a submenu opens, or when the
in-session pause menu is shown. Build/package scripts stage only the game
binary and `data.pck` for release.

### IDA labels added

- `BT3D_ShowTitleMenuBitmapAndAdvance` at `0x811`.

### Files touched

- `src/menu.c` — `BM_POD` background rendering and startup-menu MIDI
  control.
- `src/main.c` / `src/bt3d.h` — menu music lifetime/update state.
- `src/midi_player.c`, `src/midi_player.h` — TinyMidiLoader plus built-in
  synth MIDI streaming.
- `src/third_party/tinymidiloader/tml.h` — header-only MIDI parser.
- `CMakeLists.txt`, `cmake/embed_binary.cmake`, `build.sh`,
  `scripts/build-docker.sh`, `scripts/package-mac-app.sh` — embed original
  `m1.dat` at build time and stop packaging loose music assets.

## 37. Windows missing-pack startup diagnostics

The Windows `.exe` is linked as a GUI application (`-mwindows`), so stderr
messages are invisible when the player launches it by double-clicking. A
missing `data.pck` could therefore look like a crash with no useful error
even though the loader path already detected the missing archive.

The desktop missing-pack path now has an explicit Windows branch using
`MessageBoxA`. If `data.pck` is absent beside `bt3d_raylib.exe`, Windows
shows a native error dialog explaining where to place the original pack, then
the game closes the audio device and raylib window and exits with failure.
macOS and Linux keep their existing `osascript` / `zenity` / `kdialog`
branches.

This is a diagnostic test fix rather than proof of the whole Windows runtime:
it makes the most likely silent startup failure visible. If the `.exe` still
closes with `data.pck` present, the next target is instrumenting the map-load,
texture-prewarm, and audio-init sequence on a Windows machine.

### Files touched

- `src/main.c` — include `windows.h` on `_WIN32` and show `MessageBoxA` in
  the desktop missing-pack branch.
- `docs/RELEASE_NOTES.md`, `docs/WINDOWS_ZIG_BUILD.md` — document the
  Windows missing-pack dialog behavior.

## 38. Menu polish, desktop display mode, and small gameplay cleanup

The menu presentation was moved further away from the earlier dark-panel
prototype. The interactive menu now keeps the original blue `BM_POD`
background visible and draws only yellow/gold selectable buttons over it.
Menu headings are aligned to the same left edge as the buttons so `Main Menu`,
`Options`, load/save titles, and difficulty selection all line up visually.

The menu music lifetime was also expanded. The embedded `m1.dat` MIDI stream
now plays while any menu is active, including load/save/options and the
in-session pause menu, and stops when gameplay resumes. A tight-loop bug in
the MIDI renderer was fixed: sub-frame MIDI events could round down to the
current audio frame without being consumed, leaving `midi_render_frames` stuck
forever on the same event. Same-frame events are now applied immediately, and
stream updates are capped per frame so menu audio cannot monopolize the main
loop. Menu selection movement uses the short `SND_4` tick instead of silence.
The main `Campaign` entry now opens a campaign submenu with `New Game` and
`Load Game`, keeping campaign save loading out of the top-level mode list.

Desktop startup now defaults to raylib's borderless-windowed mode instead of
creating a decorated window resized to the monitor. The Options menu has a
desktop-only `Display: Borderless/Windowed` row; left/right or confirm toggles
between borderless and a centered `1280x720` window. The choice is stored in
`config.dat` as config version 2, while version 1 config files still load mouse
sensitivity and debug-tool settings and fall back to borderless.

Two small gameplay irritations were cleaned up at the same time:

- Normal gameplay no longer draws gray `status_text` popups such as door,
  pickup, ammo, and key messages over the HUD. The underlying status strings
  still exist for menu/save/debug code, and `[NOCLIP]` still appears when
  noclip is active.
- Enemy class 2 (`PRK_*`) no longer settles on `PRK_19` after dying. That
  patch only occupies the bottom quarter of the 64x64 sprite canvas, so the
  corpse appeared much smaller than the preceding death frames. The death
  animation now stops on `PRK_18`.
- The fist HUD no longer reuses the pistol's ammo counter. While the fist is
  selected, the ammo-number area is left as the normal empty HUD background.

### Files touched

- `src/menu.c` — yellow button rendering, aligned headings, music active in
  all menus, display-mode option, short move sound trigger paths.
- `src/midi_player.c` — same-frame MIDI event consumption and per-frame stream
  update cap.
- `src/platform.c`, `src/bt3d.h`, `src/main.c` — persistent desktop display
  mode and borderless/windowed application.
- `src/enemies.c` — class 2 death frame sequence.
- `src/render_ui.c` — hide weapon ammo digits while the fist is selected.

## 39. Rogue Mode prototype

Rogue Mode is now a separate main-menu start path from the campaign. It builds
an in-memory `Map242` instead of loading a `MAP_*` pack entry, then reuses the
normal runtime systems for walls, doors, markers, enemies, pickups, collision,
visibility, rendering, and audio. The generator creates a connected eastward
room chain inside the 64x64 map; opening the active exit door carves and
populates the next room. When the chain reaches the edge of the map, the run
rolls into a fresh generated wing while keeping the player's run state and
score.

Room generation now tracks explicit carved floor cells, reserved path cells,
and occupied cells. That allows more irregular room silhouettes while keeping a
guaranteed route from the entrance to the exit. Rooms can be jagged, L-shaped,
T/cross shaped, pillar-filled, or split into pockets. Enemy encounters are
weighted by depth and rough room theme, and placement uses the carved-cell list
so enemies avoid walls, the entrance, the exit, and already occupied cells.
Pickups are selected from need-aware weights: low health biases health, low
ammo biases ammo, advanced ammo follows weapon ownership, and locked rogue
exits place a reachable key before the door.

Generated rooms no longer use the same black wall texture for every surface.
Rogue room textures are selected from role-specific `STN_*` pools categorized
from original `MAP_*` usage: floor-dominant IDs for floors, ceiling-dominant
IDs for ceilings, and wall-dominant IDs for walls. Deeper rooms still get
access to a stronger wall-only industrial set, but wall-only textures are no
longer assigned to floors or ceilings. The renderer now treats generated Rogue
maps with an explicit texture layout: detail byte 0 is floor, byte 1 is wall,
byte 2 is door/wall texture backup, and byte 3 is ceiling. That keeps ceiling
textures off the floor and forces all exposed wall faces in a generated room to
use the same wall texture. Rogue walls avoid the door-like panel/stripe texture
IDs, while exit doors always use a fixed door base texture without an extra
skull overlay.

Rogue spawning now tries to protect the player from immediate enemy sightlines.
When room shape allows it, the generator places a small screen wall near the
spawn lane, and enemy placement rejects cells that have direct line-of-sight to
the player's spawn position.

Rogue exit doors no longer draw any door overlay, including the skull `VEC_1`.
They now rely on the fixed door base texture alone so generated room exits read
as normal doors.

Rogue Mode also draws compact enemy health bars above visible living enemies.
The bars are projected as a 2D overlay after the 3D world pass, so they stay
readable without changing campaign rendering.

The Rogue run counters are now attached to the top HUD instead of being drawn
as plain text below it. `Depth` was renamed to `Room`; the room number replaces
the normal campaign lives digit in Rogue Mode, and score is drawn in a compact
panel next to the HUD using the same `BM_N*` bitmap digit art as the rest of
the HUD numbers. The lives/room slot now centers its digits, and the Rogue score
panel is the same height as the HUD, joined flush to its side, and displays only
the bitmap score value without a text label. The in-game Rogue score panel was
later removed; score remains tracked and shown on the run summary screen.

Rogue enemy pacing now gates the highest enemy class with both weapon/depth
caps and a class-specific cooldown. Class 5 enemies can only enter the pool
from room 16 onward, and once one is selected the generator suppresses further
class 5 spawns for another 10 to 15 generated rooms.

The mode is intentionally arcade-style for this pass: no rogue saves are
available, death ends the run, and a summary screen shows score, best score,
rooms cleared, and kills. Score currently comes from enemy kills and room
progression, with depth scaling used for enemy mix and room bonuses. The best
score is stored in `config.dat` version 3; older config versions still load
their existing settings.

Options now expose separate music and sound-effect volume controls. Both values
are stored in `config.dat` version 4; older config versions still load their
existing settings and use full audio volume by default.

### Files touched

- `src/rogue.c` — procedural room-chain generation, wild room templates,
  runtime population, need-aware drops, weighted encounters, score tracking,
  high-score finish flow.
- `src/menu.c`, `src/main.c`, `src/combat.c` — Rogue Mode menu entry, HUD
  score display, run summary, score/death hooks, and campaign-only debug map
  cycling.
- `src/session.c`, `src/platform.c`, `src/bt3d.h` — generated-map activation
  helper, persistent rogue best score, and shared state declarations.

## 40. Rogue Mode DLC gate

Rogue Mode is now gated by an external desktop DLC marker file instead of being
always exposed in desktop executables. The code remains compiled into the
desktop binary, but the main menu only shows "Rogue Mode" when
`bt3d_rogue.dlc` exists in the executable/runtime directory and its first line
is exactly `BT3D_ROGUE_DLC_V1`.

This mirrors the strict `data.pck` runtime model: after startup changes the
process directory to the executable/app bundle location, the DLC probe checks
that directory only. Missing or invalid DLC files keep the normal campaign menu
without a disabled/locked Rogue entry. `start_rogue_game` also rejects direct
launch attempts when the DLC flag is unavailable, so menu bugs or debug paths
cannot enter Rogue Mode by accident.

Switch builds skip the DLC marker and expose Rogue Mode by default, matching
the single `.nro` distribution model.

## 41. Enemy billboard bounds

Enemy billboards now draw from their actual opaque patch bounds instead of the
full 64x64 patch rectangle. The loader records min/max opaque X/Y for each
`VEC_*` patch, and enemy sprite draw builds a cropped billboard source/size from
those bounds while preserving the sprite's camera-facing horizontal offset.

This avoids cases where one enemy standing in front of another hides the rear
enemy with empty or stray edge pixels outside the visible body. Death sprites
still use the corpse top crop, now combined with the same horizontal bounds.

## 42. Rogue room clear gate

Rogue exit doors now stay locked while any enemy in the current generated Rogue
area is alive. The check lives in `bt3d_request_door_open`, so
keyboard/gamepad use and any other door-open path share the same rule. Trying
the exit early plays the locked-door sound and shows `Clear enemies first`.

## 43. Rogue mini-level generation

Rogue Mode now generates one mini-level per depth instead of one isolated room.
Each mini-level contains 2 to 3 connected rooms, short connecting corridors,
internal doors, one guaranteed blue-key locked connector, and a final exit in
the last room. The current stable generator keeps the main route left-to-right
so door cells stay embedded in room boundary walls. The key is placed in
reachable space before the locked door, while enemy keycard drops remain
disabled in Rogue Mode.

The first room keeps the safer spawn rules, including reduced early pressure
and no direct enemy line-of-sight to spawn. Enemies and pickups are then
populated per room using the existing depth and weapon-aware Rogue pacing. The
final exit still requires every enemy in the generated mini-level to be dead.
Once that check passes, using the exit advances straight to the next generated
mini-level instead of depending on the door animation state.

Rogue enemies also gain proximity aggro within 10 tiles even without player
line-of-sight, so remaining enemies in side rooms start hunting instead of
waiting for the player to find and look at them.

Branch-room generation is currently disabled while the main 2 to 3 room path is
stabilized. Final exit doors remain embedded in the room wall and no longer
carve open floor outside the exit cell, so the advance door does not appear as
a freestanding door in empty space.

Enemy and pickup placement now use one full mini-level budget each. Enemy
classes and pickup markers spend fixed costs from their respective level
budgets, with room shares distributed across the generated rooms. Stronger
enemies and better drops therefore reduce total count instead of stacking on
top of the old per-room random counts.

Rogue difficulty now rises one step per mini-level and caps at difficulty 10.

## 44. Rogue Mode removal

Rogue Mode has been removed from the game. The build no longer compiles
`src/rogue.c`, the main menu no longer exposes a Rogue entry, the DLC marker
file is ignored, and campaign code no longer carries Rogue-specific HUD,
door, enemy, save, or generated-map branches.
Enemy/drop budgets use that capped difficulty value, so level advancement keeps
increasing pressure until the cap. Health drops also scale by difficulty:
deeper levels can roll stronger health pickups, while early levels prefer small
health unless the player is badly hurt.
