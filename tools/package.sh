#!/usr/bin/env bash
# Builds the Windows DLL, checks it and packs build-win/uaro-graphics.zip: release/ keeps it under this one name, the
# link update.bat downloads. uaro-graphics/version.txt tells which build is installed.
set -euo pipefail
cd "$(dirname "$0")/.."
version=$(sed -n 's/.*kVersion = "\(.*\)".*/\1/p' src/core/version.cpp)
cmake -S . -B build-win --no-warn-unused-cli -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-win -j >/dev/null
# Part 3: the renderer's 64-bit process.
cmake -S . -B build-win64 --no-warn-unused-cli -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x86_64.cmake \
      -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-win64 -j >/dev/null
python3 tools/check_dll.py build-win/ddraw.dll
python3 tools/check_exe.py build-win/uaro-graphics.exe
python3 tools/check_studio.py build-win64/uaro-studio.exe  # 2.3: uaRO Studio
out="build-win/uaro-graphics.zip"
stage="build-win/package"
rm -rf "$out" "$stage"
mkdir -p "$stage/uaro-graphics/remix" "$stage/uaro-graphics/ai" "$stage/uaro-graphics/pad"
cp dist/install.bat dist/uninstall.bat dist/update.bat dist/get-dlss.bat \
   dist/TEST.en.md dist/TEST.uk.md dist/TEST.ru.md "$stage/"
cp build-win/ddraw.dll build-win/uaro-graphics.exe dist/uaro-graphics.ini build-win64/uaro-render64.exe \
   build-win64/uaro-studio.exe dist/dlss-version.txt "$stage/uaro-graphics/"
cp dist/remix/rtx.conf "$stage/uaro-graphics/remix/"  # part 4: RTX Remix's settings for this game
# 1.4: uaRO Pad (github.com/kwebmn/ragnarok-gamepad), the gamepad - the Gamepad page puts it into the game's folder.
cp dist/pad/dinput.dll dist/pad/uaro-pad.ini dist/pad/version.txt "$stage/uaro-graphics/pad/"
# AI textures: the neural network's model and its weak-denoise twin, mixed by [quality] ai_denoise (Real-ESRGAN,
# BSD-3-Clause), and their licence.
cp dist/ai/realesr-general-x4v3.bin dist/ai/realesr-general-wdn-x4v3.bin dist/ai/realesr-animevideov3.bin \
   dist/ai/realesrgan-x4plus-anime-6b.bin \
   dist/ai/LICENSE-Real-ESRGAN.txt "$stage/uaro-graphics/ai/"
printf '%s build %s\r\n' "$version" "$(date '+%Y-%m-%d %H:%M')" > "$stage/uaro-graphics/version.txt"
(cd "$stage" && zip -q -r "../uaro-graphics.zip" .)
python3 tools/check_package.py "$out"
# 0.60: the setup in one file - the stub, the zip, its size (eight bytes, little-endian) and "UAROSETP".
setup="build-win/uaro-graphics-setup-full.exe"
python3 - "build-win/uaro-graphics-setup.exe" "$out" "$setup" <<'PY'
import struct, sys
stub, zipf, out = sys.argv[1:4]
data = open(zipf, "rb").read()
with open(out, "wb") as f:
    f.write(open(stub, "rb").read())
    f.write(data)
    f.write(struct.pack("<Q", len(data)) + b"UAROSETP")
PY
echo "packed $out and $setup"
