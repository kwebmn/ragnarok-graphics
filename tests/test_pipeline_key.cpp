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
    const std::string text = pipelineKeysHeader() + pipelineKeyLine(0x2453) + "zz\r\n\r\n" + pipelineKeyLine(0x2cb90);
    const std::vector<uint64_t> keys = parsePipelineKeys(text);
    CHECK_EQ(keys.size(), size_t(2));  // the line that is not hex is skipped
    CHECK(keys.size() == 2 && keys[0] == 0x2453 && keys[1] == 0x2cb90);
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
