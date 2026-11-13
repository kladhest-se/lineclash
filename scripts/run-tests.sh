#!/usr/bin/env bash
# scripts/run-tests.sh — Build and run unit and/or integration tests
# Usage:
#   ./scripts/run-tests.sh              — unit tests only
#   ./scripts/run-tests.sh --unit       — unit tests only
#   ./scripts/run-tests.sh --integration — integration tests (needs server binary)
#   ./scripts/run-tests.sh --all        — unit + integration
source "$(dirname "$0")/common.sh"

DO_UNIT=1; DO_INTEGRATION=0
for arg in "$@"; do
    case "$arg" in
        --unit)        DO_UNIT=1; DO_INTEGRATION=0 ;;
        --integration) DO_UNIT=0; DO_INTEGRATION=1 ;;
        --all)         DO_UNIT=1; DO_INTEGRATION=1 ;;
    esac
done

cd "$ROOT/tests"
make clean > /dev/null 2>&1 || true

FAILED=0

if [[ $DO_UNIT -eq 1 ]]; then
    header "Unit tests"
    make unit || FAILED=$((FAILED+1))
fi

if [[ $DO_INTEGRATION -eq 1 ]]; then
    header "Integration tests"
    SERVER="$ROOT/server/lineclashd"
    [[ -f "$SERVER" ]] || fail "server binary not found at $SERVER — build it first"
    make integration SERVER="$SERVER" || FAILED=$((FAILED+1))
fi

[[ $FAILED -eq 0 ]] && ok "All tests passed" || fail "$FAILED test suite(s) failed"
