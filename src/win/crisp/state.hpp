#pragma once
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/crisp_exe.hpp"
#include "core/glyph_cache.hpp"
#include "core/guarded.hpp"
#include "core/settled.hpp"
#include "core/twin.hpp"

// The game's methods are MSVC __thiscall: this in ECX, the callee pops its stack arguments.
#define UAGFX_THISCALL __attribute__((thiscall))

namespace uagfx::win::crisp {

// K is taken once it holds this long: not on every frame of a resize (0.5.0 review M2).
constexpr uint64_t kSettleMs = 300;

// A field of a game object.
template <typename T>
T& field(uintptr_t object, uintptr_t offset) {
    return *reinterpret_cast<T*>(object + offset);
}

// One game window picture (CDCBitmap) and its twin (spec §4).
struct Picture {
    uintptr_t self = 0;  // the CDCBitmap
    HDC dc = nullptr;
    const uint32_t* bits = nullptr;
    std::unique_ptr<TwinPicture> twin;
    struct Tile {  // the twin texture of one of the game's tiles (spec §6)
        uint32_t texture = 0;  // in the renderer; 0 = none yet
        int width = 0;
        int height = 0;
    };
    std::vector<Tile> tiles;  // by the game's tile index
};

// A tile's surface: whose tile it is.
struct TileRef {
    uintptr_t picture = 0;
    int index = 0;
};

struct State {
    bool installed = false;
    double K = 0.0;  // the twins' scale; they are kept only above 1
    uintptr_t original[crisp_exe::kBitmapSlots] = {};  // the game's methods in the vtable entries the hooks took
    uintptr_t textOutW = 0;  // the game's GDI imports before the hooks
    uintptr_t fillRgn = 0;
    uintptr_t createDibSection = 0;
    std::unordered_map<uintptr_t, std::unique_ptr<Picture>> pictures;  // by the CDCBitmap's address
    std::unordered_map<HDC, uintptr_t> dcs;                             // a picture's DC -> its CDCBitmap
    std::unordered_map<const void*, TileRef> tiles;                     // a tile's surface -> its picture
    size_t bytes = 0;  // every twin and copy together
    bool budgetLogged = false;
    std::set<uint32_t> loggedSizes;  // the safety net writes one line per window size
    Settled scale{kSettleMs};        // K once it holds
    GlyphCache glyphs{size_t(16) << 20};
    // 0.14, AI icons: the windows' icons the AI textures made larger, by the hash of the game's icon (TextureKey).
    bool aiIcons = false;
    bool aiPieces = false;  // 0.15: the windows' small pieces too
    struct Icon {
        int width = 0, height = 0;
        std::vector<uint32_t> argb;
    };
    std::unordered_map<uint64_t, Icon> icons;
    std::deque<uint64_t> iconOrder;      // the oldest first: past kIconBytes they go (asked again when drawn)
    size_t iconBytes = 0;
    std::unordered_set<uint64_t> asked;  // sent to the renderer, not back yet
    bool iconsArrived = false;           // the windows are redrawn at a present soon
    ULONGLONG iconsMarked = 0;           // when they were last
};

State& state();
bool active();  // the hooks are in place and K > 1
// The picture of a CDCBitmap: made from its pixels at first sight, made again when the address holds another
// picture now; null while crisp text is off or past the memory budget.
Picture* picture(uintptr_t bitmap);
// The same, but only for a picture that has its twin already: a fill makes none (0.5.2 review 1).
Picture* existing(uintptr_t bitmap);
void drop(uintptr_t bitmap);  // its twin, copy, tile textures and tile surfaces
// A hook's own work failed (an exception): the picture loses its twin, made again from its pixels at its next write;
// one log line.
void lost(uintptr_t bitmap) noexcept;

// text.cpp: the game's text and triangles (spec §4); the big fonts go when K changes.
BOOL WINAPI hookTextOutW(HDC dc, int x, int y, LPCWSTR text, int count);
BOOL WINAPI hookFillRgn(HDC dc, HRGN region, HBRUSH brush);
void dropFonts();

}  // namespace uagfx::win::crisp
