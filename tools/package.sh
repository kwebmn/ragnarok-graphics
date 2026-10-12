#!/usr/bin/env bash
# Builds the Windows DLL, checks it and packs build-win/uaro-graphics-<version>.zip.
set -euo pipefail
cd "$(dirname "$0")/.."
version=$(sed -n 's/.*kVersion = "\(.*\)".*/\1/p' src/core/version.cpp)
cmake -S . -B build-win --no-warn-unused-cli -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-win -j >/dev/null
python3 tools/check_dll.py build-win/ddraw.dll
out="build-win/uaro-graphics-$version.zip"
rm -f "$out"
zip -q -j "$out" build-win/ddraw.dll dist/uaro-graphics.ini dist/TEST.ru.md
echo "packed $out"
