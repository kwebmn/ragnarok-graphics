#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::win::wings {

// 3.0.18-beta.230 (the owner's wings - concept https://claude.ai/artifact/S1rYPnLYM9yByk6wz5FTok, spec
// docs/superpowers/specs/2026-11-13-wings-design.md): the big wings of whoever wears one of nine GM-only headgears
// (core/wings), drawn by the driver from uaro-graphics\wings.grf. The players' layered render (uaRO.exe FUN_005AECC0)
// hooked: at its start a wearer's wings worked out (the game's own screen point and depth scale for the character),
// its faces noted while it sorts them (noteFace, from the face sort hook); at the draws (Device7::record) the wings go
// right before its first face (facing us) or right after its last (facing away). None in «Пауза». The game's thread.
bool install();
void noteFace(uint32_t vertices);  // a face sorted (its vertices' address) - a wearer's while its render runs
void frameStart();                 // a frame shown: the next one's wearers anew

struct Quad {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // screen pixels
    uint32_t texture = 0;                  // the renderer's
    bool mirror = false;
    uint32_t tint = 0xFFFFFFFF;  // ARGB, multiplied in
};
struct Emit {
    const std::vector<Quad>* before = nullptr;  // wings to draw right before this draw of the game's
    const std::vector<Quad>* after = nullptr;   // and right after it
};
// Device7::record, once for each draw of the game's (its vertices as the game gave them)
Emit drawn(uint32_t vertices);

}  // namespace uagfx::win::wings
