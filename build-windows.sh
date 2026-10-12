#!/bin/bash
# Native MinGW/LLVM-MinGW build or cross-build from macOS/Linux.
# MINGW_BIN=/path/to/llvm-mingw/bin bash build-windows.sh
set -euo pipefail
cd "$(dirname "$0")"
compiler_dir="${MINGW_BIN:-}"
if [[ -n "$compiler_dir" ]]; then compiler_dir="${compiler_dir%/}/"; fi
cc="${compiler_dir}x86_64-w64-mingw32-clang"
cxx="${compiler_dir}x86_64-w64-mingw32-clang++"
if ! command -v "$cc" >/dev/null; then
    cc="${compiler_dir}x86_64-w64-mingw32-gcc"
    cxx="${compiler_dir}x86_64-w64-mingw32-g++"
fi
command -v "$cc" >/dev/null || { echo 'Install LLVM-MinGW or MinGW-w64 and set MINGW_BIN.' >&2; exit 1; }
mkdir -p build/windows/{engine,plugin} 'dist/windows/MITH Doom Resources'
sources=$(sed -n 's/^SRC_DOOM = //p' vendor/doomgeneric/doomgeneric/Makefile)
for object in $sources; do
    [[ "$object" == doomgeneric_xlib.o ]] && continue
    "$cc" -std=gnu11 -O2 -w -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 \
        -Ivendor/doomgeneric/doomgeneric -Isrc -include src/WindowsFiles.h \
        -c "vendor/doomgeneric/doomgeneric/${object%.o}.c" -o "build/windows/engine/$object"
done
"$cc" -std=gnu11 -O2 -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 \
    -Ivendor/doomgeneric/doomgeneric -Isrc -c src/EnginePlatform.c -o build/windows/engine/EnginePlatform.o
"$cc" -static -municode build/windows/engine/*.o -lwinmm -o 'dist/windows/MITH Doom Resources/DoomEngine.exe'
"$cc" -O2 -DGLEW_STATIC -DGLEW_NO_GLU -Ivendor/glew/include \
    -c vendor/glew/src/glew.c -o build/windows/plugin/glew.o
for source in src/EngineWindows.cpp src/Doom.cpp vendor/ffgl/source/lib/FFGLSDK.cpp; do
    "$cxx" -std=c++17 -O2 -DNDEBUG -DGLEW_STATIC -DGLEW_NO_GLU -D_WIN32_WINNT=0x0601 \
        -Ivendor/glew/include -Ivendor/ffgl/source/lib -Isrc \
        -c "$source" -o "build/windows/plugin/$(basename "${source%.cpp}").o"
done
"$cxx" -shared -static build/windows/plugin/*.o -lopengl32 -o 'dist/windows/MITH Doom.dll'
cp LICENSE THIRD_PARTY_NOTICES.md 'dist/windows/MITH Doom Resources/'
printf 'Built %s/dist/windows/MITH Doom.dll\n' "$PWD"
