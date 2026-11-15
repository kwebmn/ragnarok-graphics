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

constexpr uint32_t kFaceWords = gamemodels::kCardFaceWords;  // five uint4 a face (model_vs)

template <typename T>
void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

}  // namespace

void Renderer11::addGameModel(const gamemodels::Node& n) {
    if (n.map != modelsMap_) {  // a new map's first: the last map's let go
        modelCards_.clear();
        modelFacesUp_ = 0;
        modelsMap_ = n.map;
    }
    modelCards_.add(n);  // 3.0.20-beta.11: core/game_models_card - coloured at once when the colours are known
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
        const uint64_t key = gamemodels::colouringKey(frame);
        if (key != modelCards_.colouredWith()) {
            modelCards_.colour(gamemodels::colouringOf(frame), key);
            modelFacesUp_ = 0;
        }
        const uint32_t faces = modelCards_.faces();
        if (faces && ensureModelFaces(faces)) {
            if (modelFacesUp_ < faces) {  // the faces not on the card yet
                const D3D11_BOX box{modelFacesUp_ * kFaceWords * 4, 0, 0, faces * kFaceWords * 4, 1, 1};
                const uint32_t* fresh = modelCards_.words().data() + size_t(modelFacesUp_) * kFaceWords;
                ctx_->UpdateSubresource(modelFaces_, 0, &box, fresh, 0, 0);
                modelFacesUp_ = faces;
            }
            modelCards_.frameOf(frame.ids, modelFrame_);  // the frame's faces by texture, the textures first met
            const uint64_t nodes = modelFrame_.nodes, unknown = modelFrame_.unknown;
            modelInstanceData_.clear();
            modelDraws_.clear();
            modelRec_.reset();
            DrawSetup st = modelLike_;
            st.cull = d3d7::CULL_NONE;  // culled in model_vs as the game culls
            const uint32_t state = modelRec_.addState(st);
            static const uint8_t kNone[3 * 32] = {};  // the record's draw: three vertices of nothing (model_vs reads)
            const bool leavesApart = leavesOwnWay();  // 3.0.20-beta.13: drawn as the game's trees (drawGameLeaves11)
            for (size_t t = 0; t < modelFrame_.textures.size(); ++t) {
                const std::vector<uint32_t>& list = modelFrame_.faces[t];
                if (list.empty() || (leavesApart && leafyTexture(modelFrame_.textures[t]))) continue;
                modelDraws_.push_back(ModelDraw11{uint32_t(modelInstanceData_.size()), uint32_t(list.size())});
                modelInstanceData_.insert(modelInstanceData_.end(), list.begin(), list.end());
                DrawCmd cmd;
                cmd.state = state;
                cmd.texture[0] = modelFrame_.textures[t];
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
                noteModelCasters();  // 3.0.20-beta.12: the reflections and the torches' shadows take them
                ++modelsDrawnFrames_;
                modelsDrawnFaces_ += modelInstanceData_.size();
                modelsDrawnNodes_ += nodes;
                modelsUnknown_ += unknown;
            }
        }
    }
    if (ready) drawGameLeaves11();  // 3.0.20-beta.13
    modelsNow_.drawn(ready);  // the driver's word: from the present on
}

// 3.0.20-beta.13 (the owner: «не качаются деревья»): the frame's game models' leaves drawn as the game's own trees - a
// draw a node's leafy texture, placed by the processor (gamemodels::place), coloured as the game, through the world's
// path (not the model pass): the wind's sway, the crowns' volume, the light through, the tints, the casters
void Renderer11::drawGameLeaves11() {
    const gamemodels::Frame& frame = modelsNow_.frame();
    if (!leavesOwnWay() || frame.ids.empty() || !modelLikeKnown_ || !cameraValid_) return;
    gamemodels::Picture pic;
    if (!own_ground::viewOf(viewToWorld_, pic.view)) return;
    pic.p = camera::projectionFor(gameWidth_, gameHeight_);
    pic.how = gamemodels::colouringOf(frame);
    modelFog_.fit();
    pic.fog = modelFog_.fogged() ? &modelFog_ : nullptr;
    pic.plain = modelFog_.specularAt(0.0f);
    leafRec_.reset();
    DrawSetup st = modelLike_;
    st.cull = d3d7::CULL_NONE;  // culled by place() as the game culls
    const uint32_t state = leafRec_.addState(st);
    for (const uint32_t id : frame.ids) {
        const gamemodels::Node* n = modelCards_.node(id);
        if (!n) continue;
        leafWanted_.assign(n->textures.size(), false);
        bool any = false;
        for (size_t t = 0; t < n->textures.size(); ++t)
            if (n->textures[t] && leafyTexture(n->textures[t])) leafWanted_[t] = any = true;
        if (!any) continue;
        leafOut_.clear();
        gamemodels::place(*n, pic, leafScratch_, leafOut_, &leafWanted_);
        for (size_t t = 0; t < leafOut_.textures(); ++t) {
            const std::vector<own_ground::Tl1>& v = leafOut_.vertices(t);
            if (v.empty() || v.size() > 65535) continue;
            DrawCmd cmd;
            cmd.state = state;
            cmd.texture[0] = leafOut_.texture(t);
            cmd.primitive = d3d7::PT_TRIANGLELIST;
            cmd.fvf = d3d7::FVF_TL1;
            leafRec_.draw(cmd, v.data(), uint32_t(v.size()), nullptr, 0);
        }
    }
    if (leafRec_.draws().empty()) return;
    flushBatch();  // the game's draws gathered so far are of its own vertices
    const std::vector<uint8_t>& v = leafRec_.vertexBytes();
    if (!fillBuffer(leafVertices_, leafVertexBytes_, D3D11_BIND_VERTEX_BUFFER, v.data(), v.size(),
                    bgraVertices_ ? nullptr : &leafRec_))
        return;
    std::swap(vertices_, leafVertices_);
    for (const DrawCmd& cmd : leafRec_.draws()) draw(leafRec_, cmd);  // the world's path, as the game's trees
    flushBatch();
    std::swap(vertices_, leafVertices_);
}

}  // namespace uagfx::gpu
