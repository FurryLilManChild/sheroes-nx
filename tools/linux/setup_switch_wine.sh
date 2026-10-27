#!/usr/bin/env bash
# Make a Linux devkitPro (/opt/devkitpro) out of devkitPro's Windows install,
# run under Wine. devkitPro's Linux packages are not reachable from the cloud
# machines this port is built on; Mark's C:\devkitPro is (through the device
# bridge, as an archive).
#
#   1. Copy Mark's C:\devkitPro (with switch-dev, switch-sdl2, switch-mesa and
#      switch-libdrm_nouveau installed by pacman) to $WIN (default
#      /opt/devkitpro-win).
#   2. apt-get install -y wine64 ninja-build cmake
#   3. bash tools/linux/setup_switch_wine.sh
#
# Every devkitA64 and tools .exe gets a shell wrapper of the same name under
# /opt/devkitpro; the compiler's wrapper rewrites the Z:/ paths Wine writes
# into dependency files so ninja can read them. cmake/, libnx/ and portlibs/
# are links to the Windows copy (plain files, the same on both).
#
# For the Vulkan build (switch/build.sh, the default) two more:
#   NVK_SDK (default /opt/nvk-sdk): lib/libvulkan.a, include/vulkan,
#     include/vk_video from mesa-switch's NVK, as published in
#     ghcr.io/danfromtico/switch-dev-legacy-vulkan (MIT); its libexpat.a and
#     libzstd.a go in $WIN/portlibs/switch/lib.
#   GLSLANG_DIR (default /opt/glslang-switch): glslang 15.4.0 built with this
#     toolchain (cmake -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake
#     -DENABLE_OPT=OFF -DGLSLANG_TESTS=OFF -DENABLE_GLSLANG_BINARIES=OFF).
set -euo pipefail
WIN="${WIN:-/opt/devkitpro-win}"
DKP=/opt/devkitpro
WINE="${WINE:-/usr/lib/wine/wine64}"

[ -x "$WIN/devkitA64/bin/aarch64-none-elf-gcc.exe" ] || { echo "no devkitPro in $WIN" >&2; exit 1; }
mkdir -p "$DKP/devkitA64/bin" "$DKP/tools/bin"
for d in cmake libnx portlibs; do ln -sfn "$WIN/$d" "$DKP/$d"; done

for sub in devkitA64/bin tools/bin; do
    for exe in "$WIN/$sub"/*.exe; do
        name=$(basename "$exe" .exe)
        out="$DKP/$sub/$name"
        case "$name" in
        aarch64-none-elf-gcc|aarch64-none-elf-g++|aarch64-none-elf-c++)
            cat > "$out" <<W
#!/bin/bash
# devkitPro's Windows $name under Wine; the dependency file's Z:/ paths made Linux ones for ninja.
export WINEDEBUG=-all
mf=""; prev=""
for a in "\$@"; do [ "\$prev" = "-MF" ] && mf="\$a"; prev="\$a"; done
$WINE $exe "\$@"; rc=\$?
[ -n "\$mf" ] && [ -f "\$mf" ] && sed -i 's#Z:/#/#g' "\$mf"
exit \$rc
W
            ;;
        *)
            printf '#!/bin/sh\nexport WINEDEBUG=-all\nexec %s %s "$@"\n' "$WINE" "$exe" > "$out"
            ;;
        esac
        chmod +x "$out"
    done
done
echo "wrappers in $DKP/devkitA64/bin and $DKP/tools/bin"
