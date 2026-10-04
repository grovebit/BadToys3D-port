#!/usr/bin/env bash
# Build a macOS .app bundle around the desktop binary.
# Requires the binary at build/macos-release/bt3d_raylib (or falls back to old local build dirs).
set -euo pipefail

cd "$(dirname "$0")/.."

DATA_PCK_SRC="${DATA_PCK_SRC:-../data.pck}"
BUILD_DIR=${BUILD_DIR:-build/macos-release}
if [ ! -x "$BUILD_DIR/bt3d_raylib" ]; then
    BUILD_DIR=build-release
fi
if [ ! -x "$BUILD_DIR/bt3d_raylib" ]; then
    BUILD_DIR=build
fi
if [ ! -x "$BUILD_DIR/bt3d_raylib" ]; then
    echo "bt3d_raylib binary not found. Build first with:" >&2
    echo "  cmake -S . -B build/macos-release -DBT3D_STATIC_RAYLIB=ON" >&2
    echo "  cmake --build build/macos-release" >&2
    exit 1
fi

APP_NAME="Bad Toys 3D"
APP_DIR="dist/macos/${APP_NAME}.app"
rm -rf "$APP_DIR"
rm -f "dist/macos.zip" "dist/macos/Bad Toys 3D app.zip"
mkdir -p "$APP_DIR/Contents/MacOS"
mkdir -p "$APP_DIR/Contents/Resources"

cp "$BUILD_DIR/bt3d_raylib" "$APP_DIR/Contents/MacOS/bt3d_raylib"

if [ -f bt3d.icns ]; then
    cp bt3d.icns "$APP_DIR/Contents/Resources/bt3d.icns"
fi

if [ -f "$DATA_PCK_SRC" ]; then
    if [ ! -e "dist/macos/data.pck" ] || ! cmp -s "$DATA_PCK_SRC" "dist/macos/data.pck"; then
        cp -a "$DATA_PCK_SRC" "dist/macos/data.pck"
    fi
elif [ -f "dist/shared/data.pck" ]; then
    if [ ! -e "dist/macos/data.pck" ] || ! cmp -s "dist/shared/data.pck" "dist/macos/data.pck"; then
        cp -a "dist/shared/data.pck" "dist/macos/data.pck"
    fi
fi

cat > "$APP_DIR/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>bt3d_raylib</string>
    <key>CFBundleIconFile</key>
    <string>bt3d</string>
    <key>CFBundleIdentifier</key>
    <string>com.tibosoftware.bt3d</string>
    <key>CFBundleName</key>
    <string>Bad Toys 3D</string>
    <key>CFBundleDisplayName</key>
    <string>Bad Toys 3D</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.13</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
PLIST

chmod +x "$APP_DIR/Contents/MacOS/bt3d_raylib"

echo "Built: $APP_DIR"
echo "Output folder: dist/macos"
