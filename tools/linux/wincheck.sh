#!/bin/bash
# Compile-check the Windows PC build (tools/win/build_pc.ps1's configuration)
# on a cloud Linux machine before pushing, so Mark's builds do not stop on one
# Windows-only error at a time. clang --target=x86_64-pc-windows-msvc with
# MinGW-w64's headers (apt: mingw-w64-x86-64-dev) standing in for the Visual
# C++ and Windows SDK headers, plus header-only stand-ins for Vulkan, SDL2 and
# glslang. Then a link with empty system libraries: what is left undefined
# must be Windows / C runtime functions only.
#   tools/linux/wincheck.sh [nongen|all]     (all: also the ~90 generated files, ~10 min)
# Needs out/gen (tools/regen.sh). Work folder: $WINCHECK (default ~/wincheck).
# Known differences from MSVC's headers: MinGW defines M_PI without
# _USE_MATH_DEFINES and declares clock_gettime/setenv-like extras; grep for
# POSIX-only calls (setenv, clock_gettime, pthread_*) in changed files too.
set -e
MODE=${1:-nongen}
R=$(cd "$(dirname "$0")/../.." && pwd)
X=${WINCHECK:-$HOME/wincheck}
GLSLANG_INC=${GLSLANG_INC:-/usr/include}
mkdir -p $X/inc $X/cmake/SDL2 $X/cmake/glslang $X/lib $X/stublib
if [ ! -d $X/mingw ]; then
    dpkg -s mingw-w64-x86-64-dev >/dev/null 2>&1 || apt-get install -y -q mingw-w64-x86-64-dev >/dev/null
    cp -r /usr/share/mingw-w64/include $X/mingw
    sed -i 's/__MINGW_IMPORT/extern/g' $X/mingw/time.h
    cp -r /usr/include/vulkan $X/inc/; cp -r /usr/include/vk_video $X/inc/ 2>/dev/null || true
    cp -r /usr/include/SDL2 $X/inc/; cp -r $GLSLANG_INC/glslang $X/inc/
    cat > $X/inc/SDL2/SDL_config.h <<'EOF'
#ifndef SDL_config_h_
#define SDL_config_h_
#include "SDL_platform.h"
#include <stddef.h>
#include <stdarg.h>
#define HAVE_LIBC 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_MATH_H 1
#define HAVE_FLOAT_H 1
#define HAVE_LIMITS_H 1
#define HAVE_CTYPE_H 1
#define SDL_VIDEO_DRIVER_WINDOWS 1
#define SDL_VIDEO_VULKAN 1
#endif
EOF
    printf '#define __MINGW_IMPORT __declspec(dllimport) extern\n#define _Scanf_format_string_impl_\n' > $X/pre.h
    printf 'add_library(SDL2::SDL2 INTERFACE IMPORTED)\nset_target_properties(SDL2::SDL2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "%s/inc/SDL2")\n' $X > $X/cmake/SDL2/SDL2Config.cmake
    printf 'add_library(glslang::glslang INTERFACE IMPORTED)\nadd_library(glslang::glslang-default-resource-limits INTERFACE IMPORTED)\nset_target_properties(glslang::glslang PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "%s/inc")\n' $X > $X/cmake/glslang/glslangConfig.cmake
    for l in vulkan-1 SDL2 glslang; do llvm-ar rcs $X/lib/$l.lib; done
    cat > $X/toolchain.cmake <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_C_FLAGS_INIT "-fms-compatibility-version=19.44 -isystem $X/mingw -isystem $X/inc -D__GNUC__=12 -D__GNUC_MINOR__=0 -D__MINGW_EXTENSION= -include $X/pre.h")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(Vulkan_INCLUDE_DIR $X/inc)
set(Vulkan_LIBRARY $X/lib/vulkan-1.lib)
EOF
fi
rm -rf $X/build
cmake -S $R -B $X/build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$X/toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
    -DNFSU2_VULKAN=ON -DXBOXRECOMP_DIR=$R/xboxrecomp -DNFSU2_GEN_DIR=$R/out/gen \
    -DSDL2_DIR=$X/cmake/SDL2 -Dglslang_DIR=$X/cmake/glslang > $X/cfg.log 2>&1 || { tail $X/cfg.log; exit 1; }
cd $X/build
objs=$(ninja -t targets all | grep -o '^[^:]*\.obj')
[ "$MODE" = nongen ] && objs=$(echo "$objs" | grep -v 'recomp_0[0-9]*\.c\.obj' | grep -v recomp_dispatch)
ninja -k 0 -j $(nproc) $objs > $X/compile.log 2>&1 || true
echo "compile errors: $(grep -c 'error:' $X/compile.log)"
grep "error:" $X/compile.log | sed "s|$R/||" | sort | uniq -c | sort -rn | head -40
[ "$MODE" = all ] || exit 0
ninja -k 0 -j $(nproc) $(ninja -t targets all | grep -o '^[^:]*\.lib') >/dev/null 2>&1 || true
CMD=$(ninja -t commands sheroes_recomp | tail -1 | sed 's/^: && //; s/ && :$//')
for pass in 1 2; do
    eval "$CMD -Xlinker /libpath:$X/stublib -Xlinker /errorlimit:0" > $X/link.log 2>&1 || true
    for l in $(grep -o "could not open '[^']*'" $X/link.log | sed "s/.*'\(.*\)'/\1/" | sort -u); do llvm-ar rcs $X/stublib/$l; done
done
echo "undefined at link, other than Windows / C runtime / Vulkan / SDL / glslang:"
grep -o "undefined symbol: .*" $X/link.log | sed 's/undefined symbol: //' | sort -u | grep -v "__declspec(dllimport)" |
    grep -vE "^(vk[A-Z]|SDL_|glslang_|[A-Z][A-Za-z0-9]*$|IID_|MF|D3D|XInput|XAudio|_*[a-z][a-z0-9]*f?$|_[a-z_0-9]+$|__[a-z_]+$|mainCRTStartup)" || true
