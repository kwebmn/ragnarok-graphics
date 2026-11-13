#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::shotspec {

// 3.0.11-beta.273 (the owner's feature 1, from nostalRE: the character's picture as it was in the game): a screenshot's
// target - a PNG path as before, or a path with «|x y w h» after it (shares of the picture, 0-1) for a part of it;
// a path ending «.bgra» a raw picture (kMagic, width, height, then the B8G8R8A8 rows) the driver reads back itself
// (it has no PNG reader)
// 3.0.18-beta.189 (the report check shots', gone in .248): «|/N» after it - the whole picture N times smaller a side (2-8), each pixel the
// mean of its N x N
struct Spec {
    std::string path;
    bool crop = false;
    float x = 0, y = 0, w = 1, h = 1;
    int shrink = 1;
};
constexpr uint32_t kMagic = 0x54504155;  // "UAPT"
Spec parse(const std::string& target);
std::string withCrop(const std::string& path, float x, float y, float w, float h);
std::string withShrink(const std::string& path, int n);
// A width x height B8G8R8A8 picture (rows `pitch` apart) n times smaller a side, its rows packed (w * 4 apart)
std::vector<uint8_t> shrunk(const uint8_t* bgra, int width, int height, size_t pitch, int n, int& w, int& h);
// The pixels the spec takes of a width x height picture (inside it, at least 1 x 1)
void pixels(const Spec& s, int width, int height, int& x, int& y, int& w, int& h);
// The picture (or its part) written as the spec says: PNG or .bgra
bool write(const std::string& target, const uint8_t* bgra, int width, int height, size_t pitch);
// A .bgra file read back: its B8G8R8A8 pixels as 0xAARRGGBB (opaque), width, height
bool readRaw(const std::string& bytes, std::vector<uint32_t>& argb, int& width, int& height);

}  // namespace uagfx::shotspec
