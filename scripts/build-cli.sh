#!/usr/bin/env bash
# scripts/build-cli.sh — Build lineclash-cli terminal client (native)
# Works on: any POSIX platform, no SDL2 needed
source "$(dirname "$0")/common.sh"

header "Building CLI client  (lineclash-cli)"

cd "$ROOT"
make -C clients/cli clean > /dev/null 2>&1 || true
make -C clients/cli

[[ -f "$CLI_DIR/lineclash-cli" ]] || fail "lineclash-cli not produced"
ok "lineclash-cli  $(du -sh "$CLI_DIR/lineclash-cli" | cut -f1)  [$OS/$ARCH]"
