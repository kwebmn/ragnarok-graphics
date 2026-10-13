#!/usr/bin/env bash
# Throwaway spike for stage 3 (see dpi_test.cpp): builds dpi-test.exe (32-bit, no manifest = DPI-unaware like uaRO.exe).
set -euo pipefail
cd "$(dirname "$0")"
i686-w64-mingw32-g++ -std=c++17 -O2 -Wall -Wextra -mwindows -o dpi-test.exe dpi_test.cpp \
    -static -static-libgcc -static-libstdc++ -ld3d11 -ldxgi -s
