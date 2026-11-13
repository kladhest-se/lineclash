#!/usr/bin/env bash
# scripts/build-server.sh — Build lineclashd + lineclashd-tool (native)
# Works on: Linux (openSUSE, Ubuntu, Fedora), macOS
source "$(dirname "$0")/common.sh"

header "Building server  (lineclashd + lineclashd-tool)"

cd "$ROOT"
make -C server clean > /dev/null 2>&1 || true
make server

[[ -f "$SERVER_DIR/lineclashd" ]]      || fail "lineclashd not produced"
[[ -f "$SERVER_DIR/lineclashd-tool" ]] || fail "lineclashd-tool not produced"

ok "lineclashd      $(du -sh "$SERVER_DIR/lineclashd"      | cut -f1)"
ok "lineclashd-tool $(du -sh "$SERVER_DIR/lineclashd-tool" | cut -f1)"
