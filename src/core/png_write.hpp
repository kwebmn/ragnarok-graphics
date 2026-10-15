#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx {

// 0.54: a picture as PNG (miniz's writer, opaque RGB) - the full-size screenshot (Ctrl+Shift+F12). bgra: the
// renderer's back buffer, B8G8R8A8 rows `pitch` bytes apart.
std::vector<uint8_t> pngOf(const uint8_t* bgra, int width, int height, size_t pitch);
bool writePng(const std::string& path, const uint8_t* bgra, int width, int height, size_t pitch);

}  // namespace uagfx
