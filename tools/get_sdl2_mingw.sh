#!/bin/bash
# get_sdl2_mingw.sh [dir]: fetch the SDL2 MinGW development package the Windows
# standalone build (make platform=win) links against. Default dir ~/vfg/deps,
# which is where the Makefile looks (SDL2_PREFIX overrides).
set -e
DIR=${1:-$HOME/vfg/deps}
VER=2.32.10
mkdir -p "$DIR"
cd "$DIR"
if [ ! -d "SDL2-$VER" ]; then
    curl -sL -o "SDL2-devel-$VER-mingw.tar.gz" \
        "https://github.com/libsdl-org/SDL/releases/download/release-$VER/SDL2-devel-$VER-mingw.tar.gz"
    tar xzf "SDL2-devel-$VER-mingw.tar.gz"
fi
echo "SDL2_PREFIX=$DIR/SDL2-$VER/x86_64-w64-mingw32"
