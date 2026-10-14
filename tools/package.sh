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
out="build-win/uaro-graphics.zip"
stage="build-win/package"
rm -rf "$out" "$stage"
mkdir -p "$stage/uaro-graphics"
cp dist/install.bat dist/uninstall.bat dist/use-uaro.bat dist/use-dgvoodoo.bat dist/update.bat dist/TEST.ru.md "$stage/"
cp build-win/ddraw.dll build-win/uaro-graphics.exe dist/uaro-graphics.ini build-win64/uaro-render64.exe \
   "$stage/uaro-graphics/"
printf '%s build %s\r\n' "$version" "$(date '+%Y-%m-%d %H:%M')" > "$stage/uaro-graphics/version.txt"
(cd "$stage" && zip -q -r "../uaro-graphics.zip" .)
python3 tools/check_package.py "$out"
echo "packed $out"
