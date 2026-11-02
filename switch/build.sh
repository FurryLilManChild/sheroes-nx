#!/usr/bin/env bash
# Build Sonic Heroes (Xbox) as a Switch homebrew NRO and stage an
# SD-card layout:
#
#   <SD>/switch/sheroes/sheroes.nro
#   <SD>/switch/sheroes/game/        the extracted disc (STAGE_GAME=1)
#   <SD>/switch/sheroes/save/        created on first run
#   <SD>/switch/sheroes/sheroes_env.txt   optional KEY=VALUE runtime switches
#   <SD>/switch/sheroes/sheroes_options.txt  written by the port menu (+ and -)
#   <SD>/switch/sheroes/sheroes_log.txt   written when no nxlink host is listening
#
# The game reads the unpacked disc files directly -- never the ISO.
#
# Environment:
#   DEVKITPRO        default /opt/devkitpro (switch-dev, switch-sdl2, switch-mesa);
#                    on a cloud machine: tools/linux/setup_switch_wine.sh
#   XBOXRECOMP_DIR   toolkit (default: the vendored xboxrecomp/)
#   NFSU2_GEN_DIR    lifted C from tools/regen.sh (default $REPO/out/gen)
#   NFSU2_GAME_SRC   extracted disc to stage (default $REPO/assets/game_root)
#   SD_ROOT          staging SD root (default <repo>/switch_sd)
#   BUILD_DIR        default $REPO/out/build-switch
#   JOBS             parallel compile jobs (default: nproc, capped by memory)
#   VULKAN=1         (default) the Vulkan renderer, built in BUILD_DIR (default
#                    $REPO/out/build-switch-vk) against NVK_SDK (mesa-switch
#                    NVK, default /opt/nvk-sdk) and GLSLANG_DIR (glslang for
#                    the Switch, default /opt/glslang-switch). VULKAN=0: OpenGL
#                    (switch-mesa), in out/build-switch, slower
#   LTO=1            link-time optimisation of the lifted code (NFSU2_LTO):
#                    BUILD_DIR gets a -lto suffix and the NRO is staged as
#                    sheroes[-vulkan]-lto.nro next to the normal one, for A/B.
#                    LTO_JOBS parallel link jobs (default 4, ~2 GB each)
#   GEN_PLAIN_MEM=1  the lifted code's guest memory accesses as plain loads and
#                    stores (NFSU2_GEN_PLAIN_MEM). Default 0 (volatile): plain
#                    gave no measurable fps on the Switch (2026-11-01)
#   FFMPEG_DIR       VP6-only LGPL FFmpeg for the movies (tools/build_ffmpeg_vp6.sh
#                    switch; default $REPO/out/ffmpeg-vp6-switch). Without
#                    it the lifted (slow) decoder plays them.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
TK="${XBOXRECOMP_DIR:-$REPO/xboxrecomp}"
GEN="${NFSU2_GEN_DIR:-$REPO/out/gen}"
GAME="${NFSU2_GAME_SRC:-$REPO/assets/game_root}"
SD="${SD_ROOT:-$REPO/switch_sd}"
VK="${VULKAN:-1}"
if [ "$VK" = "1" ]; then
    BUILD="${BUILD_DIR:-$REPO/out/build-switch-vk}"
    NRO_NAME=sheroes.nro
    NVK_SDK="${NVK_SDK:-/opt/nvk-sdk}"
    GLSLANG_DIR="${GLSLANG_DIR:-/opt/glslang-switch}"
    VK_ARGS=(-DNFSU2_VULKAN=ON -DNVK_SDK="$NVK_SDK"
             -Dglslang_DIR="$GLSLANG_DIR/lib/cmake/glslang")
else
    BUILD="${BUILD_DIR:-$REPO/out/build-switch}"
    NRO_NAME=sheroes-gl.nro
    VK_ARGS=(-DNFSU2_VULKAN=OFF)
fi

LTO_ARGS=(-DNFSU2_LTO=OFF)
if [ "${LTO:-0}" = "1" ]; then
    [ -z "${BUILD_DIR:-}" ] && BUILD="$BUILD-lto"
    NRO_NAME="${NRO_NAME%.nro}-lto.nro"
    LTO_ARGS=(-DNFSU2_LTO=ON -DNFSU2_LTO_JOBS="${LTO_JOBS:-4}")
fi

FFMPEG_DIR="${FFMPEG_DIR:-$REPO/out/ffmpeg-vp6-switch}"
if [ -f "$FFMPEG_DIR/lib/libavcodec.a" ]; then
    FF_ARGS=(-DNFSU2_FFMPEG_DIR="$FFMPEG_DIR")
else
    echo "warning: no FFmpeg in $FFMPEG_DIR -- movies use the lifted VP6 decoder" >&2
    FF_ARGS=(-DNFSU2_FFMPEG_DIR=)
fi

fail() { echo "error: $*" >&2; exit 1; }
[ -f "$DEVKITPRO/cmake/Switch.cmake" ] || fail "devkitPro not found at $DEVKITPRO"
[ -f "$DEVKITPRO/portlibs/switch/include/SDL2/SDL.h" ] || fail "switch-sdl2 missing"
[ -f "$GEN/recomp_funcs.h" ] || fail "no generated code in $GEN (run tools/regen.sh)"

if [ -z "${JOBS:-}" ]; then
    mem_mb=$(( $(awk '/^MemAvailable:/ {print $2}' /proc/meminfo) / 1024 ))
    JOBS=$(( mem_mb / 1500 )); [ "$JOBS" -lt 1 ] && JOBS=1
    [ "$JOBS" -gt "$(nproc)" ] && JOBS=$(nproc)
fi

cmake -S "$REPO" -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DXBOXRECOMP_DIR="$TK" -DNFSU2_GEN_DIR="$GEN" \
    -DNFSU2_SWITCH_ICON="$REPO/switch/icon.jpg" "${VK_ARGS[@]}" "${FF_ARGS[@]}" \
    "${LTO_ARGS[@]}" -DNFSU2_GEN_PLAIN_MEM="$( [ "${GEN_PLAIN_MEM:-0}" = "1" ] && echo ON || echo OFF )" >/dev/null
ninja -C "$BUILD" -j"$JOBS"

DEST="$SD/switch/sheroes"
mkdir -p "$DEST"
cp "$BUILD/sheroes_recomp.nro" "$DEST/$NRO_NAME"
if [ "${STAGE_GAME:-0}" = "1" ]; then
    [ -f "$GAME/default.xbe" ] || fail "no extracted disc in $GAME"
    # The disc is 2.9 GB: copy only what changed.
    mkdir -p "$DEST/game"
    rsync -a --size-only "$GAME/" "$DEST/game/" --exclude '*_analysis.json'
fi
echo
echo "staged $DEST/$NRO_NAME ($(stat -c %s "$DEST/$NRO_NAME") bytes)"
echo "  start it from hbmenu with title takeover (hold R on a game) for full RAM."
