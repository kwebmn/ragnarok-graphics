#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "core/palette_grf.hpp"

namespace uagfx::app {

// 2.5.19: the Service page's Palettes.grf - the palettes the server lets be worn (core/palette_grf.hpp) from a GRF,
// a zip or a folder, made on a thread of its own and written whole at the end.
struct PalettesState {
    enum Kind { Idle, Working, Done, Failed } kind = Idle;
    std::wstring out;  // the file made
    uint32_t kept = 0, hair = 0, blocks = 0;
    std::array<uint32_t, palettes::kLeftKinds> left{};
    uint64_t bytes = 0;
    double seconds = 0;
    std::string error;  // Failed: why, in English
};
// source: a .grf, .gpf or .zip, or a folder (folder true) with a palette folder in it or the palette folder itself.
void startPalettes(const std::wstring& source, bool folder, const std::wstring& out);
PalettesState palettesState();
// Whether two paths name the same file (the source would be written over while it is read).
bool samePath(const std::wstring& a, const std::wstring& b);

}  // namespace uagfx::app
