#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cstring>

#include "core/ai/kinds.hpp"
#include "core/d3d7.hpp"
#include "core/frame.hpp"
#include "core/game_camera.hpp"
#include "core/sprite_exe.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr size_t kMostStamps = 256;     // a run of sprites
constexpr float kStampBack = 3.0f;      // view units behind the feet: what stands at the character itself stays over

float depthOf(float w) { return camera::kFar / (camera::kFar - camera::kNear) * (1.0f - camera::kNear / w); }

}  // namespace

// 3.0.16-beta.25: a blended quad of the game's sprite cache that is a player's, a monster's or an NPC's (as the
// characters' shadows know them) - its source (sprite_exe::Source), 0 none
uint8_t Renderer::Impl::characterSource(const FrameRecord& rec, const DrawCmd& d, const DrawSetup& s) {
    if (s.ui || !s.blend || d.vertexCount != 4 || !d.texture[0]) return 0;
    const auto t = textures.find(d.texture[0]);
    if (t == textures.end() || t->second.spriteCells.empty() || t->second.leafy) return 0;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + 4 * stride > v.size()) return 0;
    const uint8_t* q = v.data() + d.vertexOffset;
    if (!ai::flatQuad(q, stride, d.vertexCount, d.indexCount)) return 0;
    float u0 = 0.0f, v0 = 0.0f;
    for (int i = 0; i < 4; ++i) {
        float uv[2];
        std::memcpy(uv, q + size_t(i) * stride + 24, sizeof uv);
        u0 += uv[0] * 0.25f;
        v0 += uv[1] * 0.25f;
    }
    const uint8_t source = spriteSourceAt(t->second.spriteCells, t->second.width, t->second.height, u0, v0);
    return source == uint8_t(sprite_exe::Source::Player) || source == uint8_t(sprite_exe::Source::Monster) ||
                   source == uint8_t(sprite_exe::Source::Npc)
               ? source
               : 0;
}

// 3.0.16-beta.25 (the owner: «давай» - nostalRE's body stamp): the game draws its characters writing no depth, so
// an effect it adds after them lies over the whole body - a ring's far half too. Each sprite's shape again, writing
// the depth alone (no colour: its colour times nothing added to the picture's), its holes left out, every corner at
// the depth of its feet kStampBack further: an effect after it hides where it lies behind the character.
void Renderer::Impl::noteStamp(const FrameRecord& rec, const DrawCmd& d, const DrawSetup& s) {
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const std::vector<uint16_t>& ix = rec.indices();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (stampRec.draws().size() >= kMostStamps || d.vertexOffset + size_t(d.vertexCount) * stride > v.size() ||
        size_t(d.firstIndex) + d.indexCount > ix.size())
        return;
    std::vector<uint8_t> out(v.data() + d.vertexOffset, v.data() + d.vertexOffset + size_t(d.vertexCount) * stride);
    float feetY = -1e9f, feetW = 0.0f;  // the bottom corners' (the feet's) distance
    for (uint32_t i = 0; i < d.vertexCount; ++i) {
        float xyzw[4];
        std::memcpy(xyzw, out.data() + i * stride, sizeof xyzw);
        if (!(xyzw[3] > 0.0f)) return;
        const float w = 1.0f / xyzw[3];
        if (xyzw[1] > feetY + 0.5f) {
            feetY = xyzw[1];
            feetW = w;
        } else if (xyzw[1] > feetY - 0.5f) {
            feetW = std::max(feetW, w);
        }
    }
    const float z = std::clamp(depthOf(feetW + kStampBack), 0.0f, 1.0f);
    for (uint32_t i = 0; i < d.vertexCount; ++i) std::memcpy(out.data() + i * stride + 8, &z, sizeof z);
    DrawSetup st = s;
    st.zEnable = true;
    st.zWrite = true;
    st.zFunc = d3d7::CMP_LESSEQUAL;
    st.blend = true;
    st.srcBlend = d3d7::BLEND_ZERO;  // the picture as it is
    st.dstBlend = d3d7::BLEND_ONE;
    st.alphaTest = true;  // its holes nothing
    st.alphaFunc = d3d7::CMP_GREATER;
    st.alphaRef = 0x60;
    st.fog = false;
    DrawCmd cmd = d;
    cmd.state = stampRec.addState(st);
    stampRec.draw(cmd, out.data(), d.vertexCount, d.indexCount ? ix.data() + d.firstIndex : nullptr, d.indexCount);
}

bool Renderer::Impl::applyStamps() {
    if (stampRec.draws().empty()) return true;
    crumb("the characters' stamps");
    flushDraws();  // the game's draws gathered so far are of its own vertices
    prepareGeometry(stampRec);
    const size_t n = stampRec.draws().size();
    if (vertexBase) {
        stampPass = true;
        for (const DrawCmd& cmd : stampRec.draws()) draw(stampRec, cmd);
        stampPass = false;
    }
    geometryList = 0;  // the game's record's vertices go up again with its next draw
    stampRec.reset();
    if (!stampLogged)
        LOG_INFO("body stamp: %zu characters' sprites put into the depth %.0f units behind their feet - what is drawn "
                 "after them hides behind them ([effects] body_stamp)",
                 n, double(kStampBack));
    stampLogged = true;
    return !lost;
}

}  // namespace uagfx::gpu
