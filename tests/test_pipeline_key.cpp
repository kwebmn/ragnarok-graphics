#include "core/d3d7.hpp"
#include "core/pipeline_key.hpp"
#include "core/states.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::d3d7;

TEST(pipeline_key_round_trips) {
    DrawSetup s;
    s.zEnable = true;
    s.zWrite = false;
    s.zFunc = CMP_LESSEQUAL;
    s.cull = CULL_CW;
    s.blend = true;
    s.srcBlend = BLEND_SRCALPHA;
    s.dstBlend = BLEND_INVSRCALPHA;
    const PipelineKey k = pipelineKey(s, FVF_TL2);
    CHECK(k.zEnable && !k.zWrite && k.blend && k.twoUv);
    CHECK_EQ(int(k.zFunc), int(CMP_LESSEQUAL));
    CHECK_EQ(int(k.cull), int(CULL_CW));
    CHECK_EQ(int(k.srcBlend), int(BLEND_SRCALPHA));
    CHECK_EQ(int(k.dstBlend), int(BLEND_INVSRCALPHA));
    CHECK(unpackPipelineKey(packPipelineKey(k)) == k);
    CHECK(!pipelineKey(s, FVF_TL1).twoUv);
    CHECK(packPipelineKey(pipelineKey(s, FVF_TL1)) != packPipelineKey(k));
}

TEST(pipeline_key_file_parses_its_own_lines) {
    const std::string text = pipelineKeysHeader() + pipelineKeyLine(0x2453) + "zz\r\n\r\n" + pipelineKeyLine(0x2cba0);
    const std::vector<uint64_t> keys = parsePipelineKeys(text);
    CHECK_EQ(keys.size(), size_t(2));  // the line that is not hex is skipped
    CHECK(keys.size() == 2 && keys[0] == 0x2453 && keys[1] == 0x2cba0);
    CHECK(pipelineKeyLine(0x1a2b) == "1a2b\n");
}

TEST(pipeline_key_file_of_another_version_gives_nothing) {
    CHECK(parsePipelineKeys("uaro-graphics pipelines 2\n1a2b\n").empty());
    CHECK(parsePipelineKeys("").empty());
    CHECK(parsePipelineKeys("1a2b\n").empty());  // no header
}

TEST(pipeline_key_file_skips_keys_the_renderer_cannot_make) {
    PipelineKey bad;
    bad.blend = true;
    bad.srcBlend = BLEND_BOTHSRCALPHA;  // resolveDrawSetup never leaves 12 or 13
    PipelineKey good;
    good.zEnable = true;
    good.zFunc = CMP_LESSEQUAL;
    good.cull = CULL_NONE;
    good.srcBlend = BLEND_ONE;
    good.dstBlend = BLEND_ZERO;
    const std::string text = pipelineKeysHeader() + "40000\nffffffffffffffff\n" +
                             pipelineKeyLine(packPipelineKey(bad)) + pipelineKeyLine(packPipelineKey(good));
    const std::vector<uint64_t> keys = parsePipelineKeys(text);  // bit 18 and up, all bits, blend 12: skipped
    CHECK_EQ(keys.size(), size_t(1));
    CHECK(keys.size() == 1 && keys[0] == packPipelineKey(good));
    CHECK(validPipelineKey(packPipelineKey(good)));
    CHECK(!validPipelineKey(packPipelineKey(bad)));
}

TEST(pipeline_key_file_text_lists_each_key_once) {
    CHECK(pipelineKeysText({3, 1, 3}) == pipelineKeysHeader() + "1\n3\n");
    CHECK(pipelineKeysText({}) == pipelineKeysHeader());
}

TEST(pipeline_key_every_key_the_renderer_makes_is_valid) {
    // Every D3D7 state the client can set - and values past D3D7's, which unvalidated STR effect data may hold -
    // through resolveDrawSetup: none of the keys may be rejected (0.4.2 and 0.4.3 reviews).
    const bool both[2] = {true, true};
    const uint32_t funcs[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 0x104};
    const uint32_t culls[] = {0, 1, 2, 3, 4, 0x102};
    const uint32_t factors[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0x105};
    int rejected = 0;
    for (uint32_t z = 0; z < 2; ++z)
        for (uint32_t zw = 0; zw < 2; ++zw)
            for (const uint32_t zf : funcs)
                for (const uint32_t cull : culls)
                    for (uint32_t blend = 0; blend < 2; ++blend)
                        for (const uint32_t src : factors)
                            for (const uint32_t dst : factors)
                                for (const bool alpha : {false, true}) {
                                    DeviceStates s;
                                    s.setRender(RS_ZENABLE, z);
                                    s.setRender(RS_ZWRITEENABLE, zw);
                                    s.setRender(RS_ZFUNC, zf);
                                    s.setRender(RS_CULLMODE, cull);
                                    s.setRender(RS_ALPHABLENDENABLE, blend);
                                    s.setRender(RS_SRCBLEND, src);
                                    s.setRender(RS_DESTBLEND, dst);
                                    const DrawSetup d = resolveDrawSetup(s, alpha, both);
                                    for (const uint32_t fvf : {FVF_TL1, FVF_TL2})
                                        if (!validPipelineKey(packPipelineKey(pipelineKey(d, fvf)))) ++rejected;
                                }
    CHECK_EQ(rejected, 0);
}

TEST(pipeline_key_each_form_the_renderer_never_makes_is_rejected) {
    // Keys the renderer makes, then one change each that breaks exactly one check: every check rejects its change on
    // its own (0.4.3 review: the old bad key failed on its cull mode first, six loosened checks went unnoticed).
    PipelineKey good;  // Z test, blending
    good.zEnable = true;
    good.zWrite = true;
    good.zFunc = CMP_LESSEQUAL;
    good.cull = CULL_CCW;
    good.blend = true;
    good.srcBlend = BLEND_SRCALPHA;
    good.dstBlend = BLEND_INVSRCALPHA;
    CHECK(validPipelineKey(packPipelineKey(good)));
    PipelineKey plain;  // neither, in resolveDrawSetup's forms
    plain.zFunc = CMP_ALWAYS;
    plain.cull = CULL_NONE;
    plain.srcBlend = BLEND_ONE;
    plain.dstBlend = BLEND_ZERO;
    CHECK(validPipelineKey(packPipelineKey(plain)));
    const auto rejected = [](PipelineKey k, void (*change)(PipelineKey&)) {
        change(k);
        return !validPipelineKey(packPipelineKey(k));
    };
    CHECK(rejected(good, [](PipelineKey& k) { k.cull = 0; }));                       // no such cull mode
    CHECK(rejected(good, [](PipelineKey& k) { k.zFunc = 0; }));                      // no such comparison
    CHECK(rejected(good, [](PipelineKey& k) { k.zFunc = CMP_ALWAYS + 1; }));
    CHECK(rejected(good, [](PipelineKey& k) { k.srcBlend = 0; }));                   // no such factor
    CHECK(rejected(good, [](PipelineKey& k) { k.dstBlend = 0; }));
    CHECK(rejected(good, [](PipelineKey& k) { k.srcBlend = BLEND_BOTHSRCALPHA; }));  // BOTH* are resolved
    CHECK(rejected(good, [](PipelineKey& k) { k.dstBlend = BLEND_BOTHINVSRCALPHA; }));
    CHECK(rejected(good, [](PipelineKey& k) { k.srcBlend = 14; }));                  // past D3D7's factors
    CHECK(rejected(good, [](PipelineKey& k) { k.dstBlend = 15; }));
    CHECK(rejected(plain, [](PipelineKey& k) { k.zFunc = CMP_LESSEQUAL; }));        // no Z test: ALWAYS
    CHECK(rejected(plain, [](PipelineKey& k) { k.zWrite = true; }));                 // no Z test: no Z writes
    CHECK(rejected(plain, [](PipelineKey& k) { k.srcBlend = BLEND_SRCALPHA; }));     // no blending: ONE and ZERO
    CHECK(rejected(plain, [](PipelineKey& k) { k.dstBlend = BLEND_INVSRCALPHA; }));
}
