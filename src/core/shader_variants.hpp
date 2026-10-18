#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::shader_variants {

// 2.5.16: the draw shader made for one state of the game's draws - its Direct3D 7 stages, alpha test, fog, colour key
// and interface mode compiled in as constants (shaders.cpp UAGFX_FIXED), so the compiler folds away the branches that
// choose among them. The one shader for every state has many (the lean draw shader: about 1200 instructions and 136
// branches in DXIL; a sprite's own: 43 and 2), and an old Radeon's VLIW cores spend on each (an HD 6570 drew the
// world alone at 35 frames a second). Direct3D 11 makes them as the draws come and keeps them in its shader cache.
enum class Base : uint8_t { Full, Lean, Glow, GlowLean };  // draw_ps, draw_lean_ps, draw_glow_ps, draw_glow_lean_ps

struct Key {
    Base base = Base::Full;
    uint32_t stage0[2] = {0, 0};
    uint32_t stage1[2] = {0, 0};
    uint32_t alphaTest = 0, fog = 0, colorKey = 0, uiMode = 0;
    bool operator==(const Key& o) const;
};

struct Macro {
    std::string name, value;
};

Key keyOf(Base base, const uint32_t stage0[2], const uint32_t stage1[2], uint32_t alphaTest, uint32_t fog,
          uint32_t colorKey, uint32_t uiMode);
const char* entryOf(const Key& k);           // its shader's entry point
std::vector<Macro> macrosOf(const Key& k);   // UAGFX_FIXED, UAGFX_LEAN where lean, the values
std::string nameOf(const Key& k);            // "draw_lean_ps~<the values>": the shader cache's entry
bool parseName(const std::string& name, Key& out);  // false: not a variant's name
uint64_t hashOf(const Key& k);

struct KeyHash {
    size_t operator()(const Key& k) const { return size_t(hashOf(k)); }
};

}  // namespace uagfx::shader_variants
