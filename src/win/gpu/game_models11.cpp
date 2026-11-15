// 3.0.20-beta.10 (the owner's «делай все сразу»: «Моделі нашим рендером» in Direct3D 11, the card's way): the game's
// still models the driver captured (core/game_models), kept on the card in the world's places - each face five uint4
// (model_vs in shaders.cpp: its corners' x y z with their colours, the texture coordinates, its flags) - and drawn
// from there at the world's first blended draw or the interface: the frame's faces of a texture an instance each, the
// camera, the projection and the fog in the draw's constants. The colours made as the game makes them
// (modelcensus::expected) with the frame's factors, again when those change.
#include <d3d11.h>

#include <algorithm>
#include <cstring>

#include "core/frame.hpp"
#include "win/gpu/renderer11.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

namespace {

constexpr uint32_t kFaceWords = 20;  // five uint4 a face (model_vs)

uint32_t bitsOf(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

template <typename T>
void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

// the frame's colouring as a key: its factors' bits and its way (never 0)
uint64_t keyOf(const gamemodels::Frame& f) {
    uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](uint32_t v) {
        for (int b = 0; b < 4; ++b) h = (h ^ ((v >> (8 * b)) & 0xFF)) * 1099511628211ull;
    };
    for (const float k : f.factors) mix(bitsOf(k));
    mix(f.lit ? 1u : 0u);
    return h | 1;
}

}  // namespace

void Renderer11::addGameModel(const gamemodels::Node& n) {
    if (n.map != modelsMap_) {  // a new map's first: the last map's let go
        models_.clear();
        modelFaceData_.clear();
        modelFacesUp_ = 0;
        modelsMap_ = n.map;
    }
    if (models_.count(n.id)) return;
    ModelNode11& m = models_[n.id];
    m.node = n;
    m.first = uint32_t(modelFaceData_.size() / kFaceWords);
    // its faces by texture - the game's order kept within one -, the ones of a texture the renderer has not left out
    for (size_t f = 0; f < n.faces.size(); ++f)
        if (n.faces[f].texture < n.textures.size() && n.textures[n.faces[f].texture]) m.order.push_back(uint16_t(f));
    std::stable_sort(m.order.begin(), m.order.end(), [&n](uint16_t a, uint16_t b) {
        return n.textures[n.faces[a].texture] < n.textures[n.faces[b].texture];
    });
    for (size_t k = 0; k < m.order.size(); ++k) {
        const gamemodels::Face& f = n.faces[m.order[k]];
        const uint32_t texture = n.textures[f.texture];
        if (m.runs.empty() || m.runs.back().texture != texture)
            m.runs.push_back(ModelRun11{texture, m.first + uint32_t(k), 0});
        ++m.runs.back().count;
        uint32_t words[kFaceWords] = {};
        for (int c = 0; c < 3; ++c) {
            const float* p = &n.vertices[size_t(f.v[c]) * 3];
            for (int a = 0; a < 3; ++a) words[4 * c + a] = bitsOf(p[a]);
        }
        for (int i = 0; i < 6; ++i) words[12 + i] = bitsOf(f.uv[i]);
        words[18] = ((f.flags & gamemodels::kTwoSided) ? 1u : 0u) | (n.flipped ? 2u : 0u);
        modelFaceData_.insert(modelFaceData_.end(), words, words + kFaceWords);
    }
    if (modelColoursKey_) bakeModelColours(m);
}

// a node's corners' colours as the game colours them this frame (modelHow_) - each face's words 3, 7 and 11
void Renderer11::bakeModelColours(ModelNode11& m) {
    for (size_t k = 0; k < m.order.size(); ++k) {
        const gamemodels::Face& f = m.node.faces[m.order[k]];
        uint32_t* words = modelFaceData_.data() + size_t(m.first + k) * kFaceWords;
        for (int c = 0; c < 3; ++c) words[4 * c + 3] = modelcensus::expected(modelHow_, f.colour[c], &f.lit[3 * c]);
    }
}

// room on the card for `faces` faces: grown twice at a time, everything written again then
bool Renderer11::ensureModelFaces(uint32_t faces) {
    if (modelFaces_ && modelFacesRoom_ >= faces) return true;
    uint32_t room = std::max<uint32_t>(modelFacesRoom_, 4096);
    while (room < faces) room *= 2;
    release(modelFacesSrv_);
    release(modelFaces_);
    modelFacesRoom_ = 0;
    D3D11_BUFFER_DESC d{};
    d.ByteWidth = room * kFaceWords * 4;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SHADER_RESOURCE_VIEW_DESC v{};
    v.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    v.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    v.Buffer.NumElements = room * (kFaceWords / 4);
    HRESULT hr = device_->CreateBuffer(&d, nullptr, &modelFaces_);
    if (SUCCEEDED(hr)) hr = device_->CreateShaderResourceView(modelFaces_, &v, &modelFacesSrv_);
    if (FAILED(hr)) {
        LOG_ERROR("game models: no room on the card for %u faces (0x%08lX) - left to the game", room,
                  static_cast<unsigned long>(hr));
        release(modelFacesSrv_);
        release(modelFaces_);
        return false;
    }
    modelFacesRoom_ = room;
    modelFacesUp_ = 0;
    return true;
}

// the fog of the game's world draws this picture - a few vertices of each (its specular's alpha by the view depth),
// the line fitted when our models are drawn
void Renderer11::noteModelFog(const FrameRecord& rec, const DrawCmd& d) {
    constexpr uint32_t kMostAPicture = 512, kADraw = 4;
    if (modelFogVertices_ >= kMostAPicture || !d.vertexCount) return;
    const uint32_t stride = FrameRecord::stride(d.fvf);
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    if (stride < 24 || d.vertexOffset + size_t(d.vertexCount) * stride > vb.size()) return;
    const uint32_t step = std::max<uint32_t>(1, d.vertexCount / kADraw);
    for (uint32_t i = 0; i < d.vertexCount && modelFogVertices_ < kMostAPicture; i += step) {
        float rhw;
        uint32_t specular;
        std::memcpy(&rhw, vb.data() + d.vertexOffset + size_t(i) * stride + 12, 4);
        std::memcpy(&specular, vb.data() + d.vertexOffset + size_t(i) * stride + 20, 4);
        if (rhw <= 0.0f) continue;
        modelFog_.fog(1.0f / rhw, specular);
        ++modelFogVertices_;
    }
}

void Renderer11::drawGameModels11() {
    modelsDone_ = true;
    own_ground::View view;
    const bool ready = modelLikeKnown_ && cameraValid_ && modelVs_ && modelLayout_ &&
                       own_ground::viewOf(viewToWorld_, view);
    const gamemodels::Frame& frame = modelsNow_.frame();
    if (ready && !frame.ids.empty()) {
        // the colours as the game makes them with this frame's factors (again for every node when they changed)
        const uint64_t key = keyOf(frame);
        if (key != modelColoursKey_) {
            modelColoursKey_ = key;
            modelHow_.way = frame.lit ? modelcensus::Way::Lit : modelcensus::Way::Plain;
            std::copy(frame.factors, frame.factors + 3, modelHow_.factors);
            modelHow_.alpha = 0xFF;
            for (auto& [id, m] : models_) bakeModelColours(m);
            modelFacesUp_ = 0;
        }
        const uint32_t faces = uint32_t(modelFaceData_.size() / kFaceWords);
        if (faces && ensureModelFaces(faces)) {
            if (modelFacesUp_ < faces) {  // the faces not on the card yet
                const D3D11_BOX box{modelFacesUp_ * kFaceWords * 4, 0, 0, faces * kFaceWords * 4, 1, 1};
                const uint32_t* fresh = modelFaceData_.data() + size_t(modelFacesUp_) * kFaceWords;
                ctx_->UpdateSubresource(modelFaces_, 0, &box, fresh, 0, 0);
                modelFacesUp_ = faces;
            }
            // the frame's faces by texture, the textures in the order first met
            modelTextures_.clear();
            modelTextureAt_.clear();
            for (std::vector<uint32_t>& list : modelTextureFaces_) list.clear();
            uint64_t nodes = 0, unknown = 0;
            for (const uint32_t id : frame.ids) {
                const auto it = models_.find(id);
                if (it == models_.end()) {
                    ++unknown;
                    continue;
                }
                ++nodes;
                for (const ModelRun11& r : it->second.runs) {
                    const auto [at, fresh] = modelTextureAt_.try_emplace(r.texture, uint32_t(modelTextures_.size()));
                    if (fresh) {
                        modelTextures_.push_back(r.texture);
                        if (modelTextureFaces_.size() < modelTextures_.size()) modelTextureFaces_.emplace_back();
                    }
                    std::vector<uint32_t>& list = modelTextureFaces_[at->second];
                    for (uint32_t k = 0; k < r.count; ++k) list.push_back(r.first + k);
                }
            }
            modelInstanceData_.clear();
            modelDraws_.clear();
            modelRec_.reset();
            DrawSetup st = modelLike_;
            st.cull = d3d7::CULL_NONE;  // culled in model_vs as the game culls
            const uint32_t state = modelRec_.addState(st);
            static const uint8_t kNone[3 * 32] = {};  // the record's draw: three vertices of nothing (model_vs reads)
            for (size_t t = 0; t < modelTextures_.size(); ++t) {
                const std::vector<uint32_t>& list = modelTextureFaces_[t];
                if (list.empty()) continue;
                modelDraws_.push_back(ModelDraw11{uint32_t(modelInstanceData_.size()), uint32_t(list.size())});
                modelInstanceData_.insert(modelInstanceData_.end(), list.begin(), list.end());
                DrawCmd cmd;
                cmd.state = state;
                cmd.texture[0] = modelTextures_[t];
                cmd.primitive = d3d7::PT_TRIANGLELIST;
                cmd.fvf = d3d7::FVF_TL1;
                modelRec_.draw(cmd, kNone, 3, nullptr, 0);
            }
            flushBatch();  // the game's draws gathered so far
            if (!modelDraws_.empty() &&
                fillBuffer(modelInstances_, modelInstanceBytes_, D3D11_BIND_VERTEX_BUFFER, modelInstanceData_.data(),
                           modelInstanceData_.size() * sizeof(uint32_t), nullptr)) {
                modelFog_.fit();
                modelPass_ = true;
                for (size_t n = 0; n < modelDraws_.size(); ++n) {
                    modelDraw_ = modelDraws_[n];
                    draw(modelRec_, modelRec_.draws()[n]);
                }
                flushBatch();
                modelDraw_ = ModelDraw11{};
                modelPass_ = false;
                ++modelsDrawnFrames_;
                modelsDrawnFaces_ += modelInstanceData_.size();
                modelsDrawnNodes_ += nodes;
                modelsUnknown_ += unknown;
            }
        }
    }
    modelsNow_.drawn(ready);  // the driver's word: from the present on
}

}  // namespace uagfx::gpu
