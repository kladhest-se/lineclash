#!/bin/sh
# SPDX-License-Identifier: MIT
# Simulator builds are ad-hoc signed; device provisioning stays in Xcode.
set -eu
cd "$(dirname "$0")/.."
[ "$#" -le 1 ] || { echo "Usage: $0 [simulator|device]" >&2; exit 2; }
MODE=${1:-simulator}
case "$MODE" in simulator) SDK=appletvsimulator ;; device) SDK=appletvos ;; *) echo "Usage: $0 [simulator|device]" >&2; exit 2 ;; esac
TOOLS=${TVOS_TOOLS:-"$HOME/Development/tools/tvos"}
WORK="$PWD/dist/tvos/$MODE"
PREFIX="$TOOLS/sdk/$MODE"
for tool in cmake xcrun; do command -v "$tool" >/dev/null || { echo "Install Xcode and CMake first ($tool missing)." >&2; exit 1; }; done
xcrun --sdk "$SDK" --show-sdk-path >/dev/null
mkdir -p "$TOOLS" "$WORK"
if [ ! -f "$PREFIX/lib/cmake/SDL2/SDL2Config.cmake" ]; then
    SDL_VER=2.32.10
    SDL_SHA=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
    SDL="$TOOLS/SDL2-$SDL_VER"
    if [ ! -f "$SDL/CMakeLists.txt" ]; then
        curl -fLsS "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-$SDL_VER.tar.gz" -o "$TOOLS/SDL2-$SDL_VER.tar.gz"
        printf '%s  %s\n' "$SDL_SHA" "$TOOLS/SDL2-$SDL_VER.tar.gz" | shasum -a 256 -c -
        tar -xzf "$TOOLS/SDL2-$SDL_VER.tar.gz" -C "$TOOLS"
    fi
    cmake -S "$SDL" -B "$WORK/sdl" -DCMAKE_SYSTEM_NAME=tvOS -DCMAKE_OSX_SYSROOT="$SDK" \
        -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_BUILD_TYPE=Release \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL_TESTS=OFF -DCMAKE_INSTALL_PREFIX="$PREFIX"
    cmake --build "$WORK/sdl" --parallel 8
    cmake --install "$WORK/sdl"
fi
cmake -S clients/tvos -B "$WORK/build" -DCMAKE_SYSTEM_NAME=tvOS -DCMAKE_OSX_SYSROOT="$SDK" \
    -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_BUILD_TYPE=Release \
    -DSDL2_DIR="$PREFIX/lib/cmake/SDL2"
cmake --build "$WORK/build" --parallel 8
if [ "$MODE" = simulator ]; then
    codesign --force --sign - "$WORK/build/LineClash.app"
else
    cmake -S clients/tvos -B "$WORK/xcode" -G Xcode -DCMAKE_SYSTEM_NAME=tvOS -DCMAKE_OSX_SYSROOT=appletvos \
        -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DSDL2_DIR="$PREFIX/lib/cmake/SDL2"
    echo "Unsigned device app. Open $WORK/xcode/LineClash.xcodeproj to select your signing team and install on Apple TV."
fi
echo "Built: $WORK/build/LineClash.app"
