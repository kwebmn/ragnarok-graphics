#include <string>

#include "core/shader_variants.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::shader_variants;

namespace {

Key sprite() {  // the game's sprites: the texture times the diffuse, the alpha tested at 0xCF, fog, a colour key
    Key k;
    k.base = Base::Lean;
    k.stage0[0] = 0x04000204u;
    k.stage0[1] = 0x00000002u;
    k.stage1[0] = 0x00000001u;
    k.stage1[1] = 0x00010000u;
    k.alphaTest = 0x0001cf07u;
    k.fog = 1;
    k.colorKey = 1;
    return k;
}

}  // namespace

TEST(shader_variants_a_key_is_its_shaders_macros) {
    // 2.5.16: the draw shader for one state - its values as the shader's constants (shaders.cpp UAGFX_FIXED)
    const std::vector<Macro> m = macrosOf(sprite());
    std::string all;
    for (const Macro& x : m) all += x.name + "=" + x.value + ";";
    CHECK_EQ(all, std::string("UAGFX_FIXED=1;UAGFX_LEAN=1;UAGFX_S0X=0x04000204u;UAGFX_S0Y=0x00000002u;"
                              "UAGFX_S1X=0x00000001u;UAGFX_S1Y=0x00010000u;UAGFX_ALPHA=0x0001cf07u;UAGFX_FOG=1u;"
                              "UAGFX_KEY=1u;UAGFX_UI=0u;"));
    Key glow = sprite();
    glow.base = Base::Glow;
    CHECK_EQ(std::string(entryOf(glow)), std::string("draw_glow_ps"));
    CHECK_EQ(std::string(entryOf(sprite())), std::string("draw_lean_ps"));
    bool lean = false;
    for (const Macro& x : macrosOf(glow)) lean = lean || x.name == "UAGFX_LEAN";
    CHECK(!lean);
}

TEST(shader_variants_names_go_to_the_cache_and_back) {
    // the shader cache keeps a variant under a name of its own: read back at the next start
    const Key k = sprite();
    const std::string name = nameOf(k);
    CHECK(name.compare(0, 13, "draw_lean_ps~") == 0);
    Key back;
    CHECK(parseName(name, back));
    CHECK(back == k);
    CHECK(!parseName("draw_lean_ps", back));  // the shader for every state
    CHECK(!parseName("draw_lean_ps~12", back));
    CHECK(!parseName("water_ps~" + name.substr(13), back));
    Key other = k;
    other.alphaTest = 0x0001c007u;
    CHECK(!(other == k) && nameOf(other) != name && hashOf(other) != hashOf(k));
}

TEST(shader_variants_key_of_a_draws_constants) {
    // only what the draw shader branches on: the stages, the alpha test, the fog, the colour key, the interface mode
    uint32_t s0[2] = {0x04000204u, 2u}, s1[2] = {1u, 0x10000u};
    const Key k = keyOf(Base::Full, s0, s1, 0x0001cf07u, 1u, 1u, 0u);
    CHECK(k.base == Base::Full && k.stage0[0] == s0[0] && k.stage1[1] == s1[1] && k.alphaTest == 0x0001cf07u);
    CHECK(k.fog == 1u && k.colorKey == 1u && k.uiMode == 0u);
}
