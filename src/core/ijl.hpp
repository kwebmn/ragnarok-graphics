#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace uagfx::ijl {

// 3.0.1-beta.6 (our own libraries in place of the client's old ones): Intel JPEG Library 1.5's reading and writing
// as uaRO.exe asks for them (src/clientlibs/ijl15.cpp maps its JPEG_CORE_PROPERTIES here). Its loader (0x42BF50)
// reads a JPEG from memory into a BGR DIB; its Print Screen (0x411F72) writes one. stb_image decodes,
// stb_image_write encodes.
constexpr int kOk = 0;
constexpr int kMemory = -5, kInvalidProperties = -8, kInvalidFilename = -10, kNotJpeg = -13, kFileError = -23,
              kBufferTooSmall = -28;  // IJLERR's
constexpr int kRgb = 1, kBgr = 2;       // IJL_COLOR of a DIB

// A DIB as IJL describes it: rows of width * channels bytes and padBytes more; height above 0 the top row first,
// below 0 the bottom row first (a Windows DIB)
struct Dib {
    uint8_t* bytes = nullptr;
    int width = 0, height = 0, padBytes = 0, channels = 3, color = kBgr;
};
struct Params {
    int width = 0, height = 0, channels = 0;
};
int readParams(const uint8_t* jpg, size_t size, Params& out);
int readImage(const uint8_t* jpg, size_t size, const Dib& dib);  // the DIB of the picture's size, 3 channels
int writeImage(const Dib& dib, int quality, std::vector<uint8_t>& jpg);  // quality 1-100 (IJL's 75 by default)
const char* errorText(int code);

}  // namespace uagfx::ijl
