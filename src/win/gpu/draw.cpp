#include <cstring>

#include "core/d3d7.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

void packStage(const StageSetup& st, bool keyed, uint32_t out[2]) {
    out[0] = uint32_t(st.colorOp) | uint32_t(st.colorArg1) << 8 | uint32_t(st.colorArg2) << 16 | uint32_t(st.alphaOp) << 24;
    out[1] = uint32_t(st.alphaArg1) | uint32_t(st.alphaArg2) << 8 | uint32_t(st.texCoord) << 16 | uint32_t(keyed ? 1 : 0) << 24;
}

}  // namespace

void Renderer::Impl::prepareGeometry(const FrameRecord& rec) {
    vertexBase = 0;
    indexBase = 0;
    indexBytes = 0;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    if (!v.empty()) {
        const Upload u = allocUpload(v.size(), 16);
        if (!u.cpu) return;  // no memory for this record's vertices: its draws are skipped (vertexBase 0)
        std::memcpy(u.cpu, v.data(), v.size());
        vertexBase = u.gpu;
    }
    const std::vector<uint16_t>& i = rec.indices();
    if (!i.empty()) {
        const Upload u = allocUpload(i.size() * sizeof(uint16_t), 4);
        if (!u.cpu) {
            vertexBase = 0;
            return;
        }
        indexBytes = UINT(i.size() * sizeof(uint16_t));
        std::memcpy(u.cpu, i.data(), indexBytes);
        indexBase = u.gpu;
    }
}

void Renderer::Impl::draw(const FrameRecord& rec, const DrawCmd& d) {
    if (d.vertexCount == 0 || !vertexBase) return;
    if (d.primitive != d3d7::PT_TRIANGLELIST && d.primitive != d3d7::PT_TRIANGLESTRIP) {
        static bool logged = false;  // the client only draws lists and strips (states.md §4)
        if (!logged) LOG_INFO("primitive type %u not drawn", static_cast<unsigned>(d.primitive));
        logged = true;
        return;
    }
    const DrawSetup& s = rec.states()[d.state];
    ID3D12PipelineState* pso = pipelineFor(s, d.fvf);
    if (!pso) return;
    Texture* t[2] = {nullptr, nullptr};
    for (int i = 0; i < 2; ++i) {
        auto it = textures.find(d.texture[i] ? d.texture[i] : white);
        if (it == textures.end()) it = textures.find(white);
        if (it == textures.end()) return;
        t[i] = &it->second;
        transition(t[i]->resource, t[i]->state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(s.stage[0], s.stage[1]);  // may restart the list
    bindTarget();
    if (!boundPipeline) {  // a fresh list or after present(): the root signature and its tables are gone
        list->SetGraphicsRootSignature(root);
        boundTextures = ~0ull;
        boundSamplers = ~0ull;
    }
    if (pso != boundPipeline) {
        list->SetPipelineState(pso);
        boundPipeline = pso;
    }
    const uint64_t textureKey = uint64_t(t[0]->srv) << 32 | t[1]->srv;
    if (textureKey != boundTextures) {
        list->SetGraphicsRootDescriptorTable(kRootTextures, textureTable(t[0]->srv, t[1]->srv));
        boundTextures = textureKey;
    }
    if (samplers.ptr != boundSamplers) {
        list->SetGraphicsRootDescriptorTable(kRootSamplers, samplers);
        boundSamplers = samplers.ptr;
    }
    Constants c{};
    c.targetSize[0] = float(width);
    c.targetSize[1] = float(height);
    packStage(s.stage[0], d.keyed[0], c.stage0);
    packStage(s.stage[1], d.keyed[1], c.stage1);
    c.alphaTest = s.alphaTest ? uint32_t(s.alphaFunc) | uint32_t(s.alphaRef) << 8 | 1u << 16 : 0u;
    c.fogEnable = s.fog ? 1u : 0u;
    c.fogColor[0] = float((s.fogColor >> 16) & 255) / 255.0f;
    c.fogColor[1] = float((s.fogColor >> 8) & 255) / 255.0f;
    c.fogColor[2] = float(s.fogColor & 255) / 255.0f;
    c.fogColor[3] = 1.0f;
    c.colorKey = s.colorKey ? 1u : 0u;
    c.uiSharp = s.ui && uiSharp ? 1u : 0u;
    c.scale = float(scale);
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->IASetPrimitiveTopology(d.primitive == d3d7::PT_TRIANGLESTRIP ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
                                                                       : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const UINT stride = FrameRecord::stride(d.fvf);
    D3D12_VERTEX_BUFFER_VIEW vb{vertexBase + d.vertexOffset, d.vertexCount * stride, stride};
    list->IASetVertexBuffers(0, 1, &vb);
    if (d.indexCount) {
        D3D12_INDEX_BUFFER_VIEW ib{indexBase, indexBytes, DXGI_FORMAT_R16_UINT};
        list->IASetIndexBuffer(&ib);
        list->DrawIndexedInstanced(d.indexCount, 1, d.firstIndex, 0, 0);
    } else {
        list->DrawInstanced(d.vertexCount, 1, 0, 0);
    }
}

}  // namespace uagfx::gpu
