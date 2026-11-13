#!/usr/bin/env bash
# scripts/package-macos.sh — Build LineClash.app + lineclashd.app → DMG
# Requires: macOS, clients/gui/lineclash + server/lineclashd already built
# Usage: ./scripts/package-macos.sh [--sign "Developer ID Application: Name (TEAM)"]
source "$(dirname "$0")/common.sh"

[[ "$OS" == "Darwin" ]] || { fail "macOS packaging requires macOS"; }

SIGN_IDENTITY=""
[[ "${1:-}" == "--sign" ]] && SIGN_IDENTITY="$2"

APP_NAME="LineClash"
VER_NUM="$(echo "$VERSION" | sed 's/-.*//')"
MAC_ARCH="$(uname -m)"   # arm64 or x86_64

[[ -f "$CLIENT_DIR/lineclash" ]]      || fail "client/lineclash not found — run build-client.sh first"
[[ -f "$SERVER_DIR/lineclashd" ]]     || fail "server/lineclashd not found — run build-server.sh first"

# ── LineClash.app ─────────────────────────────────────────────
header "Creating LineClash.app"
BUNDLE="$DIST_DIR/LineClash.app"
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Resources" "$BUNDLE/Contents/Frameworks"

cp "$CLIENT_DIR/lineclash" "$BUNDLE/Contents/MacOS/lineclash"
chmod +x "$BUNDLE/Contents/MacOS/lineclash"
ok "Binary copied"

cp -R "$ROOT/music" "$BUNDLE/Contents/Resources/music"
cp "$ROOT/CREDITS.md" "$BUNDLE/Contents/Resources/CREDITS.md"
cp "$ROOT/docs/licenses/Roboto-OFL.txt" "$BUNDLE/Contents/Resources/Roboto-OFL.txt"

header "Bundling SDL2"
BIN="$BUNDLE/Contents/MacOS/lineclash"
SDL2_PATH="$(otool -L "$BIN" | awk '/libSDL2/{print $1}' | head -1)"
[[ -n "$SDL2_PATH" ]] || fail "SDL2 not found in binary — was it linked?"
SDL2_REAL="$(python3 -c "import os; print(os.path.realpath('$SDL2_PATH'))" 2>/dev/null || echo "$SDL2_PATH")"
[[ -f "$SDL2_REAL" ]] || fail "SDL2 dylib not found: $SDL2_REAL"
SDL2_NAME="$(basename "$SDL2_REAL")"
cp "$SDL2_REAL" "$BUNDLE/Contents/Frameworks/$SDL2_NAME"
chmod 755 "$BUNDLE/Contents/Frameworks/$SDL2_NAME"
RPATH="@executable_path/../Frameworks/$SDL2_NAME"
install_name_tool -change "$SDL2_PATH" "$RPATH" "$BIN"
install_name_tool -id "$RPATH" "$BUNDLE/Contents/Frameworks/$SDL2_NAME"
codesign --force --sign - "$BUNDLE/Contents/Frameworks/$SDL2_NAME"
codesign --force --sign - "$BIN"
# SDL2-compat loads SDL3 dynamically rather than declaring a Mach-O import.
if strings "$SDL2_REAL" | grep 'libSDL3.dylib' >/dev/null; then
    SDL3_PATH="$(dirname "$(dirname "$(dirname "$SDL2_PATH")")")/sdl3/lib/libSDL3.dylib"
    [[ -f "$SDL3_PATH" ]] || fail "SDL2-compat needs SDL3 beside the bundled SDL2 library"
    cp -L "$SDL3_PATH" "$BUNDLE/Contents/Frameworks/libSDL3.dylib"
    chmod 755 "$BUNDLE/Contents/Frameworks/libSDL3.dylib"
    codesign --force --sign - "$BUNDLE/Contents/Frameworks/libSDL3.dylib"
fi
ok "SDL2 bundled and re-signed"

header "Writing LineClash.app Info.plist"
cat > "$BUNDLE/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
    <key>CFBundleDisplayName</key>    <string>$APP_NAME</string>
    <key>CFBundleExecutable</key>     <string>lineclash</string>
    <key>CFBundleIdentifier</key>     <string>com.lineclash.client</string>
    <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
    <key>CFBundleName</key>           <string>$APP_NAME</string>
    <key>CFBundlePackageType</key>    <string>APPL</string>
    <key>CFBundleShortVersionString</key><string>$VER_NUM</string>
    <key>CFBundleVersion</key>        <string>$VERSION</string>
    <key>CFBundleIconFile</key>       <string>lineclash</string>
    <key>LSMinimumSystemVersion</key> <string>10.13</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>NSHumanReadableCopyright</key><string>Copyright © 2024 frossmant</string>
    <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
</dict></plist>
EOF
ok "Info.plist written"

header "Generating icon"
ICONSET="$DIST_DIR/lineclash.iconset"
rm -rf "$ICONSET"; mkdir -p "$ICONSET"
python3 - "$ICONSET" << 'PYEOF'
import struct,zlib,math,os,sys
def make_png(size):
    px=[0]*(size*size*4)
    def sp(x,y,r,g,b,a=255):
        if 0<=x<size and 0<=y<size:
            i=(y*size+x)*4;px[i]=r;px[i+1]=g;px[i+2]=b;px[i+3]=a
    def fr(x1,y1,x2,y2,r,g,b):
        [sp(xx,yy,r,g,b) for yy in range(y1,y2) for xx in range(x1,x2)]
    fr(0,0,size,size,13,13,26);s=size//8;p=size//8
    fr(p,p,p+s*3,p+s,0,220,220);fr(p+s,p+s,p+s*2,p+s*2,0,220,220)
    [sp(i,p,80,255,255) for i in range(p,p+s*3)]
    [sp(i,p+s,80,255,255) or sp(p+s,i,80,255,255) for i in range(p,p+s*2)]
    r2=size//6
    for yy in range(size):
        for xx in range(size):
            dy=min(yy,size-1-yy);dx=min(xx,size-1-xx)
            if dx<r2 and dy<r2:
                d=math.sqrt((r2-dx)**2+(r2-dy)**2);i=(yy*size+xx)*4
                if d>r2:px[i+3]=0
                elif d>r2-1.5:px[i+3]=int(255*(r2-d)/1.5)
    def chunk(n,d):c=zlib.crc32(n+d)&0xffffffff;return struct.pack('>I',len(d))+n+d+struct.pack('>I',c)
    rows=b''.join(b'\x00'+bytes(px[y*size*4:(y+1)*size*4]) for y in range(size))
    ihdr=struct.pack('>IIBBBBB',size,size,8,6,0,0,0)
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',ihdr)+chunk(b'IDAT',zlib.compress(rows,9))+chunk(b'IEND',b'')
iconset=sys.argv[1]
for size,_,suffix in [(16,1,'icon_16x16'),(32,2,'icon_16x16@2x'),(32,1,'icon_32x32'),
    (64,2,'icon_32x32@2x'),(128,1,'icon_128x128'),(256,2,'icon_128x128@2x'),
    (256,1,'icon_256x256'),(512,2,'icon_256x256@2x'),(512,1,'icon_512x512'),(1024,2,'icon_512x512@2x')]:
    p=os.path.join(iconset,suffix+'.png');open(p,'wb').write(make_png(size))
PYEOF
iconutil --convert icns "$ICONSET" --output "$BUNDLE/Contents/Resources/lineclash.icns"
rm -rf "$ICONSET"
ok "lineclash.icns generated"

if [[ -n "$SIGN_IDENTITY" ]]; then
    header "Code signing with: $SIGN_IDENTITY"
    codesign --force --timestamp --options runtime --sign "$SIGN_IDENTITY" "$BUNDLE/Contents/Frameworks/"*.dylib
    codesign --force --timestamp --sign "$SIGN_IDENTITY" "$BUNDLE/Contents/MacOS/lineclash"
    codesign --force --timestamp --options runtime --sign "$SIGN_IDENTITY" "$BUNDLE"
    codesign --verify --verbose "$BUNDLE"
    ok "Signed"
else
    warn "No signing identity — users will see Gatekeeper warning on first launch."
fi

# ── DMG ────────────────────────────────────────────────────────
header "Packaging LineClash-$VERSION-macos-$MAC_ARCH.dmg"
DMG_OUT="$DIST_DIR/LineClash-$VERSION-macos-$MAC_ARCH.dmg"
ST="$DIST_DIR/macos-staging"
rm -rf "$ST"; mkdir -p "$ST/LineClash"

cp -r "$BUNDLE"     "$ST/LineClash/LineClash.app"
cp "$SERVER_DIR/lineclashd" "$ST/LineClash/lineclashd"
chmod +x "$ST/LineClash/lineclashd"

[[ -f "$SERVER_DIR/lineclashd-tool" ]] && {
    cp "$SERVER_DIR/lineclashd-tool" "$ST/LineClash/lineclashd-tool"
    chmod +x "$ST/LineClash/lineclashd-tool"
    ok "lineclashd-tool added (standalone)"
}

cp "$ROOT/LICENSE.md" "$ST/LineClash/LICENSE.md"
cp "$ROOT/CREDITS.md" "$ST/LineClash/CREDITS.md"
cp -R "$ROOT/docs" "$ST/LineClash/docs"
cp "$ROOT/server/lineclashd.cfg.example" "$ST/LineClash/lineclashd.cfg.example"

cat > "$ST/LineClash/README.txt" << EOF
LineClash v$VERSION — Multiplayer Falling Blocks (macOS $MAC_ARCH)

CLIENT:  Double-click LineClash.app — or drag to Applications.
         First launch: right-click → Open to bypass Gatekeeper.

SERVER:  Run from Terminal: ./lineclashd (port 31457).
         Admin tool: ./lineclashd-tool --help

EOF

ZIP_OUT="$DIST_DIR/LineClash-$VERSION-macos-$MAC_ARCH.zip"
[[ ! -e "$ZIP_OUT" ]] || fail "Preserve existing ZIP before rebuilding: $ZIP_OUT"
ditto -c -k --sequesterRsrc "$ST/LineClash" "$ZIP_OUT"
if command -v hdiutil &>/dev/null; then
    KB="$(du -sk "$ST" | cut -f1)"
    hdiutil create -volname "LineClash $VERSION" -srcfolder "$ST/LineClash" \
        -ov -format UDZO -size $(( (KB/1024)+32 ))m "$DMG_OUT" > /dev/null 2>&1 && {
        DSZ="$(du -sh "$DMG_OUT" | cut -f1)"
        ok "DMG: $DMG_OUT ($DSZ)"
    } || fail "hdiutil failed"
fi

[[ -f "$DMG_OUT" ]] || fail "DMG was not produced"
if [[ -n "$SIGN_IDENTITY" ]]; then
    codesign --force --timestamp --sign "$SIGN_IDENTITY" "$DMG_OUT"
    codesign --verify --strict "$DMG_OUT"
fi
rm -rf "$ST" "$BUNDLE"
hr
ok "macOS packaging complete — dist/"
ls -lh "$DIST_DIR"/LineClash-"$VERSION"-macos-"$MAC_ARCH"* 2>/dev/null | sed 's/^/    /'
