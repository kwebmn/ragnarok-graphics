#include <string>
#include <vector>

#include "core/ai/shader_set.hpp"
#include "core/layout.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.2-beta.8 (the client build session: five shaders compiled on every start, two of them the network's): the
// pack's pictures need their mip levels alone
TEST(ai_shaders_the_pack_needs_the_mips_alone) {
    CHECK(ai::shadersFor(false) == std::vector<std::string>({"ai_mip"}));
}

TEST(ai_shaders_the_networks_take_all_five) {
    const std::vector<std::string> all = {"ai_conv", "ai_shuffle", "ai_mip", "ai_alpha", "ai_place"};
    CHECK(ai::shadersFor(true) == all);
}

// Kept beside the renderer's own: the game's, the pack builder's, uaRO Studio's engine each its own
TEST(layout_ai_shaders_beside_the_shaders) {
    CHECK_EQ(layout::aiShadersBeside("C:\\uaRO\\uaro-graphics\\cache\\shaders-64.bin"),
             std::string("C:\\uaRO\\uaro-graphics\\cache\\ai-shaders-64.bin"));
    CHECK_EQ(layout::aiShadersBeside("C:\\uaRO\\uaro-graphics\\cache\\studio-shaders-64.bin"),
             std::string("C:\\uaRO\\uaro-graphics\\cache\\ai-studio-shaders-64.bin"));
    CHECK_EQ(layout::aiShadersBeside(""), std::string());
}
