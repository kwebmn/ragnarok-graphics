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
    // Every D3D7 state the client can set, through resolveDrawSetup: none of the keys may be rejected (0.4.2 review).
    const bool both[2] = {true, true};
    int rejected = 0;
    for (uint32_t z = 0; z < 2; ++z)
        for (uint32_t zw = 0; zw < 2; ++zw)
            for (uint32_t zf = CMP_NEVER; zf <= CMP_ALWAYS; ++zf)
                for (uint32_t cull = CULL_NONE; cull <= CULL_CCW; ++cull)
                    for (uint32_t blend = 0; blend < 2; ++blend)
                        for (uint32_t src = BLEND_ZERO; src <= BLEND_BOTHINVSRCALPHA; ++src)
                            for (uint32_t dst = BLEND_ZERO; dst <= BLEND_BOTHINVSRCALPHA; ++dst)
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

TEST(pipeline_key_forms_the_renderer_never_makes_are_rejected) {
    PipelineKey k;
    k.cull = CULL_NONE;
    k.srcBlend = BLEND_ONE;
    k.dstBlend = BLEND_ZERO;
    k.zFunc = CMP_LESSEQUAL;  // without a Z test resolveDrawSetup gives ALWAYS
    CHECK(!validPipelineKey(packPipelineKey(k)));
    k.zFunc = CMP_ALWAYS;
    CHECK(validPipelineKey(packPipelineKey(k)));
    k.srcBlend = BLEND_SRCALPHA;  // no blending means ONE, ZERO
    CHECK(!validPipelineKey(packPipelineKey(k)));
    k.srcBlend = BLEND_ONE;
    k.cull = 0;  // no such cull mode
    CHECK(!validPipelineKey(packPipelineKey(k)));
}
