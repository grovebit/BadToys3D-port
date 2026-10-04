# Android Port Plan

Status: shared runtime entry points and a Gradle/CMake scaffold exist. Android
is not a finished release target; storage, touch input and device validation
remain outstanding. There is no `build.sh --android` option.

This port lives in this repository as another platform target. The shared
C/raylib game runtime stays in `src/`; Android-specific build, storage, and
input code should stay behind `BT3D_PLATFORM_ANDROID` or under `android/`.

## Milestone 1: Shared Runtime Entry Points

- Move startup, per-frame update/draw, close checks, and shutdown into reusable
  `bt3d_runtime_*` functions.
- Keep desktop, web, and Switch behavior unchanged through a thin `main.c`.
- Add `BT3D_PLATFORM_ANDROID` as a CMake option and compile definition.

Status: implemented as the first Android-enabling refactor.

## Milestone 2: Android Build Skeleton

- Add an `android/` Gradle project.
- Build a native raylib Android app through Gradle + CMake.
- Reuse the root `src/` files and target definitions where practical.
- Produce a debug APK that launches to the current menu or missing-pack screen.

Expected layout:

```text
android/
  settings.gradle
  build.gradle
  app/
    build.gradle
    src/main/AndroidManifest.xml
    src/main/res/
    src/main/assets/
```

Initial build command:

```bash
cd android
gradle assembleDebug
```

The first build downloads the Android Gradle plugin and raylib 5.5. Android
Studio, the Android SDK, and the Android NDK must already be installed.
This repository does not include a Gradle wrapper yet, so either use Android
Studio's bundled Gradle support or install Gradle on `PATH`.

Expected output:

```text
android/app/build/outputs/apk/debug/app-debug.apk
```

Status: scaffold present; APK launch and device behavior still need validation.
The current Gradle configuration targets SDK 35 and the arm64-v8a ABI.

## Milestone 3: Android Storage and `data.pck`

- Decide whether `data.pck` is bundled as an APK asset or imported by the user.
- Add Android-safe data-pack candidates.
- Save `config.dat` and saves under app-writable storage.
- Keep parser/save code platform-neutral.

## Milestone 4: Gamepad Playability

- Map Android controllers through raylib gamepad APIs.
- Reuse the Switch-style control scheme where possible.
- Verify movement, look, fire, use, map, menu, confirm, and back.

## Milestone 5: Touch Playability

- Add a virtual left stick for movement.
- Add a right drag area for look.
- Add touch buttons for fire, use, map, menu, and weapon cycling.
- Keep touch-specific logic isolated from desktop input paths.

## Milestone 6: Packaging and Polish

- Add icon, label, orientation, and fullscreen manifest settings.
- Add `./build.sh --android` once Gradle builds are stable.
- Test debug/release APKs on emulator and at least one physical device.
