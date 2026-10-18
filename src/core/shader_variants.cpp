#include "core/shader_variants.hpp"

#include <cstdio>
#include <cstring>

namespace uagfx::shader_variants {
namespace {

const char* const kEntries[4] = {"draw_ps", "draw_lean_ps", "draw_glow_ps", "draw_glow_lean_ps"};

std::string hex(uint32_t v) {
    char text[16];
    std::snprintf(text, sizeof text, "0x%08xu", static_cast<unsigned>(v));
    return text;
}

}  // namespace

bool Key::operator==(const Key& o) const {
    return base == o.base && stage0[0] == o.stage0[0] && stage0[1] == o.stage0[1] && stage1[0] == o.stage1[0] &&
           stage1[1] == o.stage1[1] && alphaTest == o.alphaTest && fog == o.fog && colorKey == o.colorKey &&
           uiMode == o.uiMode;
}

Key keyOf(Base base, const uint32_t stage0[2], const uint32_t stage1[2], uint32_t alphaTest, uint32_t fog,
          uint32_t colorKey, uint32_t uiMode) {
    Key k;
    k.base = base;
    k.stage0[0] = stage0[0];
    k.stage0[1] = stage0[1];
    k.stage1[0] = stage1[0];
    k.stage1[1] = stage1[1];
    k.alphaTest = alphaTest;
    k.fog = fog;
    k.colorKey = colorKey;
    k.uiMode = uiMode;
    return k;
}

const char* entryOf(const Key& k) { return kEntries[int(k.base) & 3]; }

std::vector<Macro> macrosOf(const Key& k) {
    std::vector<Macro> m{{"UAGFX_FIXED", "1"}};
    if (k.base == Base::Lean || k.base == Base::GlowLean) m.push_back({"UAGFX_LEAN", "1"});
    m.push_back({"UAGFX_S0X", hex(k.stage0[0])});
    m.push_back({"UAGFX_S0Y", hex(k.stage0[1])});
    m.push_back({"UAGFX_S1X", hex(k.stage1[0])});
    m.push_back({"UAGFX_S1Y", hex(k.stage1[1])});
    m.push_back({"UAGFX_ALPHA", hex(k.alphaTest)});
    m.push_back({"UAGFX_FOG", std::to_string(k.fog) + "u"});
    m.push_back({"UAGFX_KEY", std::to_string(k.colorKey) + "u"});
    m.push_back({"UAGFX_UI", std::to_string(k.uiMode) + "u"});
    return m;
}

std::string nameOf(const Key& k) {
    char text[128];
    std::snprintf(text, sizeof text, "%s~%08x.%08x.%08x.%08x.%08x.%x.%x.%x", entryOf(k),
                  static_cast<unsigned>(k.stage0[0]), static_cast<unsigned>(k.stage0[1]),
                  static_cast<unsigned>(k.stage1[0]), static_cast<unsigned>(k.stage1[1]),
                  static_cast<unsigned>(k.alphaTest), static_cast<unsigned>(k.fog), static_cast<unsigned>(k.colorKey),
                  static_cast<unsigned>(k.uiMode));
    return text;
}

bool parseName(const std::string& name, Key& out) {
    const size_t tilde = name.find('~');
    if (tilde == std::string::npos) return false;
    const std::string entry = name.substr(0, tilde);
    int base = -1;
    for (int i = 0; i < 4; ++i)
        if (entry == kEntries[i]) base = i;
    if (base < 0) return false;
    unsigned v[8] = {};
    int used = 0;
    if (std::sscanf(name.c_str() + tilde + 1, "%8x.%8x.%8x.%8x.%8x.%x.%x.%x%n", &v[0], &v[1], &v[2], &v[3], &v[4],
                    &v[5], &v[6], &v[7], &used) != 8 ||
        size_t(used) != name.size() - tilde - 1)
        return false;
    Key k;
    k.base = Base(base);
    k.stage0[0] = v[0];
    k.stage0[1] = v[1];
    k.stage1[0] = v[2];
    k.stage1[1] = v[3];
    k.alphaTest = v[4];
    k.fog = v[5];
    k.colorKey = v[6];
    k.uiMode = v[7];
    if (nameOf(k) != name) return false;
    out = k;
    return true;
}

uint64_t hashOf(const Key& k) {
    const uint32_t v[9] = {uint32_t(k.base), k.stage0[0], k.stage0[1], k.stage1[0], k.stage1[1],
                           k.alphaTest,      k.fog,       k.colorKey,  k.uiMode};
    uint64_t h = 1469598103934665603ull;  // FNV-1a
    for (const uint32_t x : v)
        for (int b = 0; b < 4; ++b) {
            h ^= (x >> (8 * b)) & 255u;
            h *= 1099511628211ull;
        }
    return h;
}

}  // namespace uagfx::shader_variants
