// 3.0.19-beta.50 (the owner: «3 можем сделать?» - the game's faces handed over in batches): the ground's cells come as
// one draw each (uaRO.exe FUN_0040BF60: a 4-vertex strip, FVF 0x2C4), thousands a frame; those in a row with the same
// textures and states made one triangle list - each strip's two triangles as the card would draw them
#include <cstdint>
#include <vector>

#include "core/config.hpp"
#include "core/live_config.hpp"
#include "core/quad_batch.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(quad_batch_strips_into_one_list) {
    std::vector<uint8_t> verts;
    std::vector<uint16_t> idx;
    uint8_t quad[4 * 40];
    for (int i = 0; i < 4 * 40; ++i) quad[i] = uint8_t(i);
    quadbatch::append(verts, idx, quad, 40);
    quadbatch::append(verts, idx, quad, 40);
    CHECK(verts.size() == 2 * 4 * 40 && verts[40] == quad[40] && verts[160 + 41] == quad[41]);
    // a strip's triangles (0 1 2) and (1 2 3) - the second one's turn reversed, as the card draws it: (2 1 3)
    const std::vector<uint16_t> want = {0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7};
    CHECK(idx == want);
    CHECK(quadbatch::isBatch(8, 12) && quadbatch::isBatch(4, 6));
    CHECK(!quadbatch::isBatch(4, 0) && !quadbatch::isBatch(8, 11) && !quadbatch::isBatch(6, 9));
    CHECK(quadbatch::kMostQuads * 4 <= 65536u);  // 16-bit indices
}

// [quality] batch_ground - on unless the file says 0; shown on «Якість», applied at once
TEST(config_batch_ground_on_by_default) {
    CHECK(parseConfig("").batchGround);
    CHECK(!parseConfig("[quality]\nbatch_ground = 0\n").batchGround);
    const settings::IniSetting* row = settings::findSetting("quality.batch_ground");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Quality && settings::tokenOf(*row, parseConfig("")) == "1");
    CHECK(appliesNow("quality.batch_ground"));
    CHECK(!liveUpdate(parseConfig(""), parseConfig("[quality]\nbatch_ground = 0\n")).next.batchGround);
}
