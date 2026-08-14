#!/usr/bin/env bash
# scripts/common.sh — Shared helpers sourced by all build/package scripts
# Usage: source "$(dirname "$0")/common.sh"

set -euo pipefail

# ── Paths ─────────────────────────────────────────────────────
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="$ROOT/dist"
CLIENT_DIR="$ROOT/clients/gui"
SERVER_DIR="$ROOT/server"
CLI_DIR="$ROOT/clients/cli"
SHARED_DIR="$ROOT/shared"

# ── Colours ───────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
ok()      { echo -e "${GREEN}  ✓${RESET}  $*"; }
fail()    { echo -e "${RED}  ✗  $*${RESET}" >&2; exit 1; }
info()    { echo -e "${CYAN}[INFO]${RESET} $*"; }
warn()    { echo -e "${YELLOW}[WARN]${RESET} $*"; }
header()  { echo -e "\n${BOLD}==> $*${RESET}"; }
hr()      { echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"; }

# ── Version ───────────────────────────────────────────────────
VERSION="$(grep -m1 '^## v' "$ROOT/CHANGELOG.md" 2>/dev/null \
    | sed 's/^## v//' | sed 's/[[:space:]].*$//' || echo '0.9')"
BUILD_DATE="$(date -u +%Y-%m-%d)"

# Build number — incremented each compile, stored in build_number.txt
bn=0
[[ -f "$ROOT/build_number.txt" ]] && bn=$(cat "$ROOT/build_number.txt")
bn=$((bn+1))
echo "$bn" > "$ROOT/build_number.txt"
printf "/* auto */\n#define BUILD_NUMBER %d\n#define BUILD_DATE \"%s\"\n" \
    "$bn" "$BUILD_DATE" > "$CLIENT_DIR/src/buildnum.h"
printf "/* auto */\n#define BUILD_NUMBER %d\n#define BUILD_DATE \"%s\"\n" \
    "$bn" "$BUILD_DATE" > "$SERVER_DIR/src/buildnum.h"

# ── Platform detection ────────────────────────────────────────
OS="$(uname -s)"   # Darwin / Linux
ARCH="$(uname -m)" # arm64 / x86_64 / aarch64

# Normalise arch
case "$ARCH" in
    aarch64) ARCH="arm64" ;;
    x86_64)  ARCH="x86_64" ;;
esac

mkdir -p "$DIST_DIR"

info "LineClash v$VERSION  |  $OS/$ARCH  |  build $bn"
