#!/usr/bin/env sh
set -eu

ARCH="${1:-arm64}"
VERSION="${VERSION:-0.7.0}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP_ROOT="$ROOT/dist/native-macos-$ARCH"
APP="$APP_ROOT/Codex Quota Dock.app"
BIN="$APP/Contents/MacOS/CodexQuotaDock"

rm -rf "$APP_ROOT"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

cd "$ROOT/native/macos/CodexQuotaDock"
if [ "$ARCH" = "universal" ]; then
  swift build -c release --arch arm64
  swift build -c release --arch x86_64
  lipo -create \
    ".build/arm64-apple-macosx/release/CodexQuotaDock" \
    ".build/x86_64-apple-macosx/release/CodexQuotaDock" \
    -output "$BIN"
else
  swift build -c release --arch "$ARCH"
  cp ".build/$ARCH-apple-macosx/release/CodexQuotaDock" "$BIN"
fi

ICONSET="$APP_ROOT/AppIcon.iconset"
mkdir -p "$ICONSET"
sips -z 16 16 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_16x16.png" >/dev/null
sips -z 32 32 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_16x16@2x.png" >/dev/null
sips -z 32 32 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_32x32.png" >/dev/null
sips -z 64 64 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_32x32@2x.png" >/dev/null
sips -z 128 128 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_128x128.png" >/dev/null
sips -z 256 256 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_128x128@2x.png" >/dev/null
sips -z 256 256 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_256x256.png" >/dev/null
sips -z 512 512 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_256x256@2x.png" >/dev/null
sips -z 512 512 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_512x512.png" >/dev/null
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/AppIcon.icns"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleExecutable</key><string>CodexQuotaDock</string>
  <key>CFBundleIdentifier</key><string>io.github.fearofmissingout.codex-quota-dock.native</string>
  <key>CFBundleName</key><string>Codex Quota Dock</string>
  <key>CFBundleDisplayName</key><string>Codex Quota Dock</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>CFBundleIconFile</key><string>AppIcon</string>
  <key>LSUIElement</key><true/>
  <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST

codesign --force --deep --sign - "$APP"
ditto -c -k --sequesterRsrc --keepParent "$APP" "$ROOT/dist/codex-quota-dock-native-macos-$ARCH.zip"
echo "Built $ROOT/dist/codex-quota-dock-native-macos-$ARCH.zip"
