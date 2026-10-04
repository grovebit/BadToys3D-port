# Current release notes

This describes the current source tree, not a separately versioned release.
For earlier changes, see the [changelog](CHANGELOG.md).

## Platforms and packaging

- Desktop: macOS, Linux and Windows. [Distribution scripts](DIST_BUILD.md)
  package a macOS app and stage the Windows executable.
- Nintendo Switch: NRO with an icon and embedded romfs when assets are supplied.
- Browser: C/raylib compiled to WASM, with pack selection and IndexedDB saves.
- Android: build scaffold only; storage, touch controls and device validation
  remain in the [port plan](ANDROID_PORT_PLAN.md).

Supply the original `data.pck`. Scripts copy it into distribution directories
when available; it is not tracked in Git. Desktop loads it beside the executable
or macOS app bundle. Missing data is shown in-game.

## Runtime and gameplay

- Assets are decoded once at startup. Visible world geometry is culled and
  batched by texture; enemy pathfinding and visibility results are cached.
- Keyboard, mouse and gamepad input share the runtime. See the
  [controls](SETUP.md#controls).
- Three difficulty levels scale enemy health, damage and timing; see the
  [difficulty tables](DIFFICULTY_TABLES.md).
- Death restarts the map with the starting weapons. Losing the last life
  restarts the campaign; level exits clear keys and quicksave.
- Desktop targets 144 FPS; web follows `requestAnimationFrame`. Switch has
  no explicit frame cap (display synchronization still applies).

## Saves and limitations

Save format is **3** and config format is **5**. Older versions are ignored.
Save files contain raw C structs and are not portable between builds with
different struct layouts.

Saves are written to a sibling `.tmp` file, then replace the previous save only
after writing and closing succeed. Failed writes preserve the previous save;
an interrupted write's temporary file is reused on the next attempt. This does
not guarantee durability through power loss. Browser persistence also depends
on successful IndexedDB sync. Failed loads preserve the active game state.

The [test suite](DEVELOPMENT.md#tests) covers parsers and, on native macOS/Linux,
headless save/load and gameplay regressions. Platform builds and interactive
play still need their own verification.
