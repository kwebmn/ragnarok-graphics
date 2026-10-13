#include "core/d3d7.hpp"
#include "core/states.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::d3d7;

namespace {
const bool kBoth[2] = {true, true};
const bool kNone[2] = {false, false};

// What the client sets once at start-up (states.md §1-2, FUN_00403f30).
DeviceStates clientInit() {
    DeviceStates s;
    s.setRender(RS_CULLMODE, CULL_NONE);
    s.setRender(RS_ZFUNC, CMP_LESSEQUAL);
    s.setRender(RS_ALPHAFUNC, CMP_GREATEREQUAL);
    s.setRender(RS_ALPHAREF, 0xF0);
    s.setRender(RS_COLORKEYENABLE, 1);
    s.setRender(RS_ZENABLE, 1);
    s.setRender(RS_ZWRITEENABLE, 1);
    s.setRender(RS_SRCBLEND, BLEND_SRCALPHA);
    s.setRender(RS_DESTBLEND, BLEND_INVSRCALPHA);
    s.setStage(0, TSS_COLOROP, TOP_MODULATE);
    s.setStage(0, TSS_COLORARG1, TA_TEXTURE);
    s.setStage(0, TSS_COLORARG2, TA_DIFFUSE);
    s.setStage(0, TSS_ALPHAOP, TOP_MODULATE);
    s.setStage(0, TSS_ALPHAARG1, TA_TEXTURE);
    s.setStage(0, TSS_ALPHAARG2, TA_DIFFUSE);
    s.setStage(0, TSS_ADDRESS, TADDRESS_CLAMP);
    s.setStage(0, TSS_MAGFILTER, TFG_LINEAR);
    s.setStage(0, TSS_MINFILTER, TFN_LINEAR);
    s.setStage(1, TSS_COLOROP, TOP_MODULATEALPHA_ADDCOLOR);
    s.setStage(1, TSS_COLORARG1, TA_TEXTURE);
    s.setStage(1, TSS_COLORARG2, TA_CURRENT);
    s.setStage(1, TSS_ALPHAOP, TOP_DISABLE);
    s.setStage(1, TSS_ADDRESS, TADDRESS_BORDER);
    s.setStage(1, TSS_BORDERCOLOR, 0xFF000000);
    return s;
}
}  // namespace

TEST(states_d3d7_defaults) {
    const DeviceStates s;
    CHECK_EQ(s.render(RS_ZENABLE), 0u);
    CHECK_EQ(s.render(RS_ZWRITEENABLE), 1u);
    CHECK_EQ(s.render(RS_SRCBLEND), uint32_t(BLEND_ONE));
    CHECK_EQ(s.render(RS_DESTBLEND), uint32_t(BLEND_ZERO));
    CHECK_EQ(s.render(RS_CULLMODE), uint32_t(CULL_CCW));
    CHECK_EQ(s.render(RS_ZFUNC), uint32_t(CMP_LESSEQUAL));
    CHECK_EQ(s.render(RS_ALPHAFUNC), uint32_t(CMP_ALWAYS));
    CHECK_EQ(s.render(RS_FOGENABLE), 0u);
    CHECK_EQ(s.stage(0, TSS_COLOROP), uint32_t(TOP_MODULATE));
    CHECK_EQ(s.stage(0, TSS_ALPHAOP), uint32_t(TOP_SELECTARG1));
    CHECK_EQ(s.stage(1, TSS_COLOROP), uint32_t(TOP_DISABLE));
    CHECK_EQ(s.stage(1, TSS_TEXCOORDINDEX), 1u);
    CHECK_EQ(s.stage(0, TSS_ADDRESSU), uint32_t(TADDRESS_WRAP));
    CHECK_EQ(s.stage(0, TSS_MAGFILTER), uint32_t(TFG_POINT));
    CHECK_EQ(s.stage(0, TSS_MIPFILTER), uint32_t(TFP_NONE));
}

TEST(states_address_sets_both_axes_and_out_of_range_is_ignored) {
    DeviceStates s;
    s.setStage(0, TSS_ADDRESS, TADDRESS_CLAMP);
    CHECK_EQ(s.stage(0, TSS_ADDRESSU), uint32_t(TADDRESS_CLAMP));
    CHECK_EQ(s.stage(0, TSS_ADDRESSV), uint32_t(TADDRESS_CLAMP));
    s.setRender(999, 5);
    CHECK_EQ(s.render(999), 0u);
    s.setStage(9, TSS_COLOROP, TOP_ADD);
    CHECK_EQ(s.stage(9, TSS_COLOROP), 0u);
}

TEST(states_ui_list) {
    DeviceStates s = clientInit();
    s.setRender(RS_ZENABLE, 0);
    s.setRender(RS_ALPHATESTENABLE, 1);
    s.setRender(RS_ALPHAREF, 1);
    s.setRender(RS_ALPHABLENDENABLE, 1);
    s.setStage(1, TSS_COLOROP, TOP_DISABLE);
    const DrawSetup d = resolveDrawSetup(s, false, kBoth);
    CHECK(!d.zEnable);
    CHECK(!d.zWrite);
    CHECK(d.blend);
    CHECK_EQ(d.srcBlend, uint8_t(BLEND_SRCALPHA));
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_INVSRCALPHA));
    CHECK(d.alphaTest);
    CHECK_EQ(d.alphaFunc, uint8_t(CMP_GREATEREQUAL));
    CHECK_EQ(d.alphaRef, uint8_t(1));
    CHECK(d.colorKey);
    CHECK_EQ(d.cull, uint8_t(CULL_NONE));
    CHECK_EQ(d.stage[0].colorOp, uint8_t(TOP_MODULATE));
    CHECK_EQ(d.stage[0].addressU, uint8_t(TADDRESS_CLAMP));
    CHECK_EQ(d.stage[1].colorOp, uint8_t(TOP_DISABLE));
}

TEST(states_both_blends_expand_and_dest_alpha_needs_target_alpha) {
    DeviceStates s = clientInit();
    s.setRender(RS_ALPHABLENDENABLE, 1);
    s.setRender(RS_SRCBLEND, BLEND_BOTHSRCALPHA);
    s.setRender(RS_DESTBLEND, BLEND_ZERO);
    DrawSetup d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.srcBlend, uint8_t(BLEND_SRCALPHA));
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_INVSRCALPHA));
    s.setRender(RS_SRCBLEND, BLEND_BOTHINVSRCALPHA);
    d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.srcBlend, uint8_t(BLEND_INVSRCALPHA));
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_SRCALPHA));
    // The STR effects' favourite pair: on a target without alpha DESTALPHA reads 1 -> additive.
    s.setRender(RS_SRCBLEND, BLEND_SRCALPHA);
    s.setRender(RS_DESTBLEND, BLEND_DESTALPHA);
    d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_ONE));
    s.setRender(RS_DESTBLEND, BLEND_INVDESTALPHA);
    d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_ZERO));
    d = resolveDrawSetup(s, true, kBoth);
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_INVDESTALPHA));
}

TEST(states_disabled_parts_are_normalised) {
    DeviceStates a = clientInit();
    a.setRender(RS_ALPHABLENDENABLE, 0);
    a.setRender(RS_SRCBLEND, BLEND_SRCALPHA);
    a.setStage(1, TSS_COLOROP, TOP_DISABLE);
    DeviceStates b = a;
    b.setRender(RS_SRCBLEND, BLEND_ONE);              // ignored: blending is off
    b.setStage(1, TSS_COLORARG1, TA_DIFFUSE);          // ignored: stage 1 is off
    b.setRender(RS_ALPHAREF, 0x55);                    // ignored: alpha test is off
    CHECK(resolveDrawSetup(a, false, kBoth) == resolveDrawSetup(b, false, kBoth));
    DeviceStates c = a;
    c.setStage(0, TSS_COLOROP, TOP_DISABLE);           // stage 0 off turns every later stage off
    c.setStage(1, TSS_COLOROP, TOP_ADD);
    const DrawSetup d = resolveDrawSetup(c, false, kBoth);
    CHECK_EQ(d.stage[0].colorOp, uint8_t(TOP_DISABLE));
    CHECK_EQ(d.stage[1].colorOp, uint8_t(TOP_DISABLE));
}

TEST(states_stage1_without_texture_passes_through) {
    // Calibration leaves stage 1 on MODULATEALPHA_ADDCOLOR(TEXTURE, CURRENT) with no texture bound.
    const DeviceStates s = clientInit();
    const bool onlyStage0[2] = {true, false};
    const DrawSetup d = resolveDrawSetup(s, false, onlyStage0);
    CHECK_EQ(d.stage[1].colorOp, uint8_t(TOP_DISABLE));
    // Stage 0 without a texture keeps its operation: the renderer binds a white texture.
    const DrawSetup e = resolveDrawSetup(s, false, kNone);
    CHECK_EQ(e.stage[0].colorOp, uint8_t(TOP_MODULATE));
}

TEST(states_vertex_fog) {
    DeviceStates s = clientInit();
    s.setRender(RS_FOGENABLE, 1);
    s.setRender(RS_FOGCOLOR, 0xFF6699CC);
    s.setRender(RS_FOGTABLEMODE, FOG_NONE);
    DrawSetup d = resolveDrawSetup(s, false, kBoth);
    CHECK(d.fog);
    CHECK_EQ(d.fogColor, 0xFF6699CCu);
    s.setRender(RS_FOGENABLE, 0);
    d = resolveDrawSetup(s, false, kBoth);
    CHECK(!d.fog);
    CHECK_EQ(d.fogColor, 0u);
}

TEST(states_both_blends_as_dest_read_as_their_source_half) {
    DeviceStates s = clientInit();
    s.setRender(RS_ALPHABLENDENABLE, 1);
    s.setRender(RS_SRCBLEND, BLEND_ONE);
    s.setRender(RS_DESTBLEND, BLEND_BOTHSRCALPHA);  // STR effect data is not validated (states.md §3)
    DrawSetup d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.srcBlend, uint8_t(BLEND_ONE));
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_SRCALPHA));
    s.setRender(RS_DESTBLEND, BLEND_BOTHINVSRCALPHA);
    d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_INVSRCALPHA));
}

TEST(states_values_past_d3d7_draw_as_the_defaults) {
    // STR effect data is not validated (states.md §3): a comparison, cull mode or blend factor D3D7 does not have is
    // drawn as ALWAYS, NONE and ONE / ZERO - not as a pipeline that cannot be made (0.4.3 review).
    DeviceStates s = clientInit();
    s.setRender(RS_ZFUNC, 9);
    s.setRender(RS_CULLMODE, 0);
    s.setRender(RS_ALPHABLENDENABLE, 1);
    s.setRender(RS_SRCBLEND, 0);
    s.setRender(RS_DESTBLEND, 14);
    DrawSetup d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.zFunc, uint8_t(CMP_ALWAYS));
    CHECK_EQ(d.cull, uint8_t(CULL_NONE));
    CHECK_EQ(d.srcBlend, uint8_t(BLEND_ONE));
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_ZERO));
    s.setRender(RS_ZFUNC, 0x104);  // LESSEQUAL in its low byte only
    s.setRender(RS_CULLMODE, 0x102);
    s.setRender(RS_SRCBLEND, 0x105);
    s.setRender(RS_DESTBLEND, 15);
    d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.zFunc, uint8_t(CMP_ALWAYS));
    CHECK_EQ(d.cull, uint8_t(CULL_NONE));
    CHECK_EQ(d.srcBlend, uint8_t(BLEND_ONE));
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_ZERO));
}
