#include <windows.h>

#include <d3d11.h>

#include <algorithm>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/frame.hpp"
#include "core/game_camera.hpp"
#include "win/gpu/renderer11.hpp"
#include "win/log.hpp"

// 3.0.16-beta.35 (the owner: «перенести тени от факелов и отпечаток тел героев на D3D11»): [effects] body_stamp with
// Direct3D 11 - Direct3D 12's body_stamp.cpp: the game draws its characters writing no depth, so an effect after them
// lies over the whole body. Each sprite's shape again, writing the depth alone (its colour times nothing added), its
// holes left out, every corner at the depth of its feet kStampBack further; drawn through draw() from their own
// buffers (applySilhouette's way) once a draw of something else comes.
namespace uagfx::gpu {
namespace {

constexpr size_t kMostStamps = 256;
constexpr float kStampBack = 3.0f;

float depthOf(float w) { return camera::kFar / (camera::kFar - camera::kNear) * (1.0f - camera::kNear / w); }

}  // namespace

void Renderer11::noteStamp(const FrameRecord& rec, const DrawCmd& d, const DrawSetup& s) {
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const std::vector<uint16_t>& ix = rec.indices();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (stampRec_.draws().size() >= kMostStamps || d.vertexOffset + size_t(d.vertexCount) * stride > v.size() ||
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
    cmd.state = stampRec_.addState(st);
    stampRec_.draw(cmd, out.data(), d.vertexCount, d.indexCount ? ix.data() + d.firstIndex : nullptr, d.indexCount);
}

void Renderer11::applyStamps() {
    if (stampRec_.draws().empty()) return;
    flushBatch();  // the game's draws gathered so far are of its own vertices
    const std::vector<uint8_t>& v = stampRec_.vertexBytes();
    const std::vector<uint16_t>& ix = stampRec_.indices();
    const size_t n = stampRec_.draws().size();
    if (fillBuffer(seamVertices_, seamVertexBytes_, D3D11_BIND_VERTEX_BUFFER, v.data(), v.size(),
                   bgraVertices_ ? nullptr : &stampRec_) &&
        (ix.empty() || fillBuffer(seamIndices_, seamIndexBytes_, D3D11_BIND_INDEX_BUFFER, ix.data(),
                                  ix.size() * sizeof(uint16_t), nullptr))) {
        std::swap(vertices_, seamVertices_);
        std::swap(indices_, seamIndices_);
        stampPass_ = true;
        for (const DrawCmd& cmd : stampRec_.draws()) draw(stampRec_, cmd);
        flushBatch();
        stampPass_ = false;
        std::swap(vertices_, seamVertices_);
        std::swap(indices_, seamIndices_);
        if (!stampLogged_)
            LOG_INFO("body stamp: %zu characters' sprites put into the depth %.0f units behind their feet - what is "
                     "drawn after them hides behind them (Direct3D 11, [effects] body_stamp)",
                     n, double(kStampBack));
        stampLogged_ = true;
    }
    stampRec_.reset();
}

}  // namespace uagfx::gpu
