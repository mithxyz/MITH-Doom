#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
bundle="dist/MITH Doom.bundle"
mkdir -p "$bundle/Contents/MacOS" "$bundle/Contents/Resources" build
if [[ ! -f src/Splash.h ]]; then python3 tests/make_splash.py; fi
for arch in arm64 x86_64; do
    mkdir -p "build/$arch/engine" "build/$arch/plugin"
    # Use the exact upstream core source list, replacing only its window backend.
    sources=$(sed -n 's/^SRC_DOOM = //p' vendor/doomgeneric/doomgeneric/Makefile)
    for object in $sources; do
        [[ "$object" == doomgeneric_xlib.o ]] && continue
        source="vendor/doomgeneric/doomgeneric/${object%.o}.c"
        clang -std=gnu11 -O2 -w -D_DEFAULT_SOURCE -D__MACOSX__ -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 \
            -arch "$arch" -mmacosx-version-min=11.0 -Ivendor/doomgeneric/doomgeneric -Isrc \
            -c "$source" -o "build/$arch/engine/$object"
    done
    clang -std=gnu11 -O2 -arch "$arch" -mmacosx-version-min=11.0 -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 \
        -Ivendor/doomgeneric/doomgeneric -Isrc -c src/EnginePlatform.c -o "build/$arch/engine/EnginePlatform.o"
    clang -arch "$arch" -mmacosx-version-min=11.0 build/"$arch"/engine/*.o -framework CoreFoundation -o "build/$arch/DoomEngine"
    for source in src/Engine.cpp src/Doom.cpp vendor/ffgl/source/lib/FFGLSDK.cpp; do
        clang++ -std=c++17 -O2 -Wno-deprecated-declarations -arch "$arch" -mmacosx-version-min=11.0 \
            -Ivendor/ffgl/source/lib -Isrc -c "$source" -o "build/$arch/plugin/$(basename "${source%.cpp}").o"
    done
    clang++ -bundle -arch "$arch" -mmacosx-version-min=11.0 build/"$arch"/plugin/*.o -framework OpenGL -o "build/$arch/MITH Doom"
done
lipo -create build/arm64/DoomEngine build/x86_64/DoomEngine -output "$bundle/Contents/Resources/DoomEngine"
lipo -create "build/arm64/MITH Doom" "build/x86_64/MITH Doom" -output "$bundle/Contents/MacOS/MITH Doom"
cp LICENSE THIRD_PARTY_NOTICES.md "$bundle/Contents/Resources/"
cat > "$bundle/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>MITH Doom</string>
<key>CFBundleIdentifier</key><string>com.mith.doom</string>
<key>CFBundleName</key><string>MITH Doom</string>
<key>CFBundlePackageType</key><string>BNDL</string>
<key>CFBundleShortVersionString</key><string>0.1.1</string>
<key>CFBundleVersion</key><string>2</string>
<key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict></plist>
PLIST
codesign --force --sign - "$bundle/Contents/Resources/DoomEngine"
codesign --force --sign - "$bundle"
printf 'Built %s\n' "$PWD/$bundle"
