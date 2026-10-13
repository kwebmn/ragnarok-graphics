#include "core/states.hpp"

namespace uagfx {

using namespace d3d7;

namespace {

uint8_t noDestAlpha(uint8_t factor) {
    if (factor == BLEND_DESTALPHA) return BLEND_ONE;
    if (factor == BLEND_INVDESTALPHA) return BLEND_ZERO;
    return factor;
}

bool usesTexture(uint32_t op, uint32_t arg1, uint32_t arg2) {
    if (op == TOP_DISABLE) return false;
    const bool a1 = (arg1 & TA_SELECTMASK) == TA_TEXTURE;
    const bool a2 = (arg2 & TA_SELECTMASK) == TA_TEXTURE;
    if (op == TOP_SELECTARG1) return a1;
    if (op == TOP_SELECTARG2) return a2;
    return a1 || a2 || op == TOP_BLENDTEXTUREALPHA || op == TOP_BLENDTEXTUREALPHAPM;
}

StageSetup readStage(const DeviceStates& s, uint32_t i) {
    StageSetup st;
    st.colorOp = uint8_t(s.stage(i, TSS_COLOROP));
    st.colorArg1 = uint8_t(s.stage(i, TSS_COLORARG1));
    st.colorArg2 = uint8_t(s.stage(i, TSS_COLORARG2));
    st.alphaOp = uint8_t(s.stage(i, TSS_ALPHAOP));
    st.alphaArg1 = uint8_t(s.stage(i, TSS_ALPHAARG1));
    st.alphaArg2 = uint8_t(s.stage(i, TSS_ALPHAARG2));
    st.texCoord = uint8_t(s.stage(i, TSS_TEXCOORDINDEX) & 7);
    st.addressU = uint8_t(s.stage(i, TSS_ADDRESSU));
    st.addressV = uint8_t(s.stage(i, TSS_ADDRESSV));
    st.magFilter = uint8_t(s.stage(i, TSS_MAGFILTER));
    st.minFilter = uint8_t(s.stage(i, TSS_MINFILTER));
    st.mipFilter = uint8_t(s.stage(i, TSS_MIPFILTER));
    st.borderColor = s.stage(i, TSS_BORDERCOLOR);
    return st;
}

}  // namespace

DeviceStates::DeviceStates() {
    rs_[RS_TEXTUREPERSPECTIVE] = 1;
    rs_[RS_ZENABLE] = 0;
    rs_[RS_FILLMODE] = 3;   // D3DFILL_SOLID
    rs_[RS_SHADEMODE] = 2;  // D3DSHADE_GOURAUD
    rs_[RS_ZWRITEENABLE] = 1;
    rs_[RS_LASTPIXEL] = 1;
    rs_[RS_SRCBLEND] = BLEND_ONE;
    rs_[RS_DESTBLEND] = BLEND_ZERO;
    rs_[RS_CULLMODE] = CULL_CCW;
    rs_[RS_ZFUNC] = CMP_LESSEQUAL;
    rs_[RS_ALPHAFUNC] = CMP_ALWAYS;
    rs_[RS_FOGEND] = 0x3F800000;      // 1.0f
    rs_[RS_FOGDENSITY] = 0x3F800000;  // 1.0f
    rs_[RS_TEXTUREFACTOR] = 0xFFFFFFFF;
    rs_[RS_CLIPPING] = 1;
    rs_[RS_LIGHTING] = 1;
    rs_[RS_COLORVERTEX] = 1;
    for (uint32_t i = 0; i < kStages; ++i) {
        auto& t = ts_[i];
        t[TSS_COLOROP] = i == 0 ? TOP_MODULATE : TOP_DISABLE;
        t[TSS_COLORARG1] = TA_TEXTURE;
        t[TSS_COLORARG2] = TA_CURRENT;
        t[TSS_ALPHAOP] = i == 0 ? TOP_SELECTARG1 : TOP_DISABLE;
        t[TSS_ALPHAARG1] = TA_TEXTURE;
        t[TSS_ALPHAARG2] = TA_CURRENT;
        t[TSS_TEXCOORDINDEX] = i;
        t[TSS_ADDRESS] = TADDRESS_WRAP;
        t[TSS_ADDRESSU] = TADDRESS_WRAP;
        t[TSS_ADDRESSV] = TADDRESS_WRAP;
        t[TSS_MAGFILTER] = TFG_POINT;
        t[TSS_MINFILTER] = TFN_POINT;
        t[TSS_MIPFILTER] = TFP_NONE;
        t[TSS_MAXANISOTROPY] = 1;
    }
}

void DeviceStates::setRender(uint32_t state, uint32_t value) {
    if (state < kRenderStateCount) rs_[state] = value;
}

uint32_t DeviceStates::render(uint32_t state) const { return state < kRenderStateCount ? rs_[state] : 0; }

void DeviceStates::setStage(uint32_t stage, uint32_t type, uint32_t value) {
    if (stage >= kStages || type >= kStageStateCount) return;
    ts_[stage][type] = value;
    if (type == TSS_ADDRESS) {
        ts_[stage][TSS_ADDRESSU] = value;
        ts_[stage][TSS_ADDRESSV] = value;
    }
}

uint32_t DeviceStates::stage(uint32_t stage, uint32_t type) const {
    return stage < kStages && type < kStageStateCount ? ts_[stage][type] : 0;
}

bool StageSetup::operator==(const StageSetup& o) const {
    return colorOp == o.colorOp && colorArg1 == o.colorArg1 && colorArg2 == o.colorArg2 && alphaOp == o.alphaOp &&
           alphaArg1 == o.alphaArg1 && alphaArg2 == o.alphaArg2 && texCoord == o.texCoord && addressU == o.addressU &&
           addressV == o.addressV && magFilter == o.magFilter && minFilter == o.minFilter && mipFilter == o.mipFilter &&
           borderColor == o.borderColor;
}

bool DrawSetup::operator==(const DrawSetup& o) const {
    return zEnable == o.zEnable && zWrite == o.zWrite && zFunc == o.zFunc && cull == o.cull && blend == o.blend &&
           srcBlend == o.srcBlend && dstBlend == o.dstBlend && alphaTest == o.alphaTest && alphaFunc == o.alphaFunc &&
           alphaRef == o.alphaRef && fog == o.fog && fogColor == o.fogColor && colorKey == o.colorKey &&
           stage[0] == o.stage[0] && stage[1] == o.stage[1];
}

DrawSetup resolveDrawSetup(const DeviceStates& s, bool targetHasAlpha, const bool stageHasTexture[2]) {
    DrawSetup d;
    d.zEnable = s.render(RS_ZENABLE) != 0;
    d.zWrite = d.zEnable && s.render(RS_ZWRITEENABLE) != 0;
    d.zFunc = d.zEnable ? uint8_t(s.render(RS_ZFUNC)) : uint8_t(CMP_ALWAYS);
    d.cull = uint8_t(s.render(RS_CULLMODE));

    d.blend = s.render(RS_ALPHABLENDENABLE) != 0;
    if (d.blend) {
        uint8_t src = uint8_t(s.render(RS_SRCBLEND));
        uint8_t dst = uint8_t(s.render(RS_DESTBLEND));
        if (src == BLEND_BOTHSRCALPHA) {
            src = BLEND_SRCALPHA;
            dst = BLEND_INVSRCALPHA;
        } else if (src == BLEND_BOTHINVSRCALPHA) {
            src = BLEND_INVSRCALPHA;
            dst = BLEND_SRCALPHA;
        }
        // BOTH* is defined for SRCBLEND only. As a DESTBLEND (unvalidated STR effect data, states.md §3) it reads as
        // its source half, as in Wine; passed through, D3D12 would refuse the pipeline and the draw would vanish.
        if (dst == BLEND_BOTHSRCALPHA) dst = BLEND_SRCALPHA;
        if (dst == BLEND_BOTHINVSRCALPHA) dst = BLEND_INVSRCALPHA;
        if (!targetHasAlpha) {
            src = noDestAlpha(src);
            dst = noDestAlpha(dst);
        }
        d.srcBlend = src;
        d.dstBlend = dst;
    }

    d.alphaTest = s.render(RS_ALPHATESTENABLE) != 0;
    if (d.alphaTest) {
        d.alphaFunc = uint8_t(s.render(RS_ALPHAFUNC));
        d.alphaRef = uint8_t(s.render(RS_ALPHAREF) & 0xFF);
    }

    d.fog = s.render(RS_FOGENABLE) != 0 && s.render(RS_FOGTABLEMODE) == FOG_NONE;
    if (d.fog) d.fogColor = s.render(RS_FOGCOLOR);
    d.colorKey = s.render(RS_COLORKEYENABLE) != 0;

    for (uint32_t i = 0; i < 2; ++i) {
        StageSetup st = readStage(s, i);
        bool off = st.colorOp == TOP_DISABLE || (i > 0 && d.stage[i - 1].colorOp == TOP_DISABLE);
        // A later stage with nothing bound that wants a texture passes CURRENT through (what Wine does too);
        // stage 0 gets a white texture from the renderer instead.
        if (!off && i > 0 && !stageHasTexture[i] &&
            (usesTexture(st.colorOp, st.colorArg1, st.colorArg2) || usesTexture(st.alphaOp, st.alphaArg1, st.alphaArg2)))
            off = true;
        d.stage[i] = off ? StageSetup{} : st;
    }
    return d;
}

}  // namespace uagfx
