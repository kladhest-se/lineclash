#!/usr/bin/env bash
# Native build + runtime verification. Run in a clean checkout per platform.
set -euo pipefail
cd "$(dirname "$0")/.."
platform=$(uname -s)
mkdir -p dist/verification
log="dist/verification/${platform}.log"
exec > >(tee "$log") 2>&1
printf 'Platform: %s\n' "$platform"
${CC:-cc} --version
"${PYTHON:-python3}" --version
make release-tools-test PYTHON="${PYTHON:-python3}"
"${PYTHON:-python3}" tests/test_directory.py
"${PYTHON:-python3}" tests/test_web_refresh.py
make clean
case "$platform" in
    MINGW*|MSYS*|CYGWIN*) make -j"${JOBS:-4}" server client ;;
    *) make -j"${JOBS:-4}" ;;
esac
make -C tests unit integration gui music music-ui discovery PYTHON="${PYTHON:-python3}"
printf 'PASS: native build and runtime verification on %s\n' "$platform"
