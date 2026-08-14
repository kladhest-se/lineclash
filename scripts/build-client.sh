#!/usr/bin/env bash
# scripts/build-client.sh — Build lineclash SDL2 GUI client (native)
# Works on: macOS (Homebrew SDL2), Linux (system SDL2)
source "$(dirname "$0")/common.sh"

header "Building GUI client  (lineclash)"

# Ensure SDL2 is available
if [[ "$OS" == "Darwin" ]]; then
    command -v brew &>/dev/null || fail "Homebrew not found"
    brew list sdl2 &>/dev/null || brew install sdl2
elif command -v zypper &>/dev/null; then
    sudo zypper install -y libSDL2-devel 2>/dev/null || true
elif command -v apt-get &>/dev/null; then
    sudo apt-get install -y libsdl2-dev 2>/dev/null || true
elif command -v dnf &>/dev/null; then
    sudo dnf install -y SDL2-devel 2>/dev/null || true
fi

cd "$ROOT"
make -C clients/gui clean > /dev/null 2>&1 || true
make -C clients/gui

[[ -f "$CLIENT_DIR/lineclash" ]] || fail "lineclash binary not produced"
ok "lineclash  $(du -sh "$CLIENT_DIR/lineclash" | cut -f1)  [$OS/$ARCH]"
