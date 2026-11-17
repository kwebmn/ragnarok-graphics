// 3.0.20-beta.10 (the owner's «делай все сразу»: «Моделі нашим рендером» in Direct3D 11, the card's way): the game's
// still models the driver captured (core/game_models), kept on the card in the world's places - each face five uint4
// (model_vs in shaders.cpp: its corners' x y z with their colours, the texture coordinates, its flags) - and drawn
// from there at the world's first blended draw or the interface: the frame's faces of a texture an instance each, the
// camera, the projection and the fog in the draw's constants. The colours made as the game makes them
// (modelcensus::expected) with the frame's factors, again when those change.
#include <d3d11.h>

#include <algorithm>
#include <cstring>

#include "core/model_gaps.hpp"
#include "core/daylight.hpp"
#include "core/foliage.hpp"
#include "core/frame.hpp"
#include "win/gpu/draw_constants.hpp"
#include "win/gpu/renderer11.hpp"
#include "win/gpu/threads.hpp"
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
    // 3.0.20-beta.56 (R-2BX4BG, [view3d] close_gaps; as Direct3D 12's): a still model's gaps closed as it is kept
    if (effects_.closeGaps && !n.moving && !modelCards_.node(n.id)) {
        gamemodels::Node closed = n;
        if (const size_t added = model_gaps::close(closed)) {
            static bool told = false;
            if (!told)
                LOG_INFO("game models: a node's gaps closed (a model made for the view from above), %zu faces added "
                         "- and so every such node from now ([view3d] close_gaps, Direct3D 11)", added);
            told = true;
            modelCards_.add(closed);
            return;
        }
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
    // 3.0.20-beta.14 (stage 4): after the faces, the frame's moving nodes' matrices - three uint4 each; 3.0.20-beta.34
    // and the leaves' camera (leaf_vs)
    d.ByteWidth = room * kFaceWords * 4 + gamemodels::kCardMostMoving * 48 + 48;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SHADER_RESOURCE_VIEW_DESC v{};
    v.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    v.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    v.Buffer.NumElements = room * (kFaceWords / 4) + gamemodels::kCardMostMoving * 3 + 3;
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

// 3.0.20-beta.14 (stage 4): the frame's node `i` - its still ones, then its moving ones (`toView` its matrix this
// frame); none when unknown, or a still one listed as moving or the other way round
const gamemodels::Node* Renderer11::modelNodeOf(const gamemodels::Frame& frame, size_t i, const float*& toView) const {
    const bool still = i < frame.ids.size();
    const size_t k = still ? 0 : i - frame.ids.size();
    toView = nullptr;
    if (!still && (k >= frame.moving.size() || frame.matrices.size() < 12 * (k + 1))) return nullptr;
    const gamemodels::Node* n = modelCards_.node(still ? frame.ids[i] : frame.moving[k]);
    if (!n || n->moving == still) return nullptr;
    if (!still) toView = &frame.matrices[12 * k];
    return n;
}

void Renderer11::drawGameModels11() {
    modelsDone_ = true;
    modelCameraAt_ = modelMatricesKept_ = 0;  // 3.0.20-beta.34: none this frame till put up below
    own_ground::View view;
    // 3.0.20-beta.14: a map's faces past kCardMostFaces leave no room for a face's place in the batches - the game's
    const bool ready = modelLikeKnown_ && cameraValid_ && modelVs_ && modelLayout_ &&
                       modelCards_.faces() < gamemodels::kCardMostFaces && own_ground::viewOf(viewToWorld_, view);
    const gamemodels::Frame& frame = modelsNow_.frame();
    if (ready && !frame.empty()) {
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
            // 3.0.20-beta.14 (stage 4): the frame's moving nodes' matrices after the faces (holeG: their first uint4),
            // their faces into the batches with their places in the list - model_vs puts them by those
            const size_t moving = std::min<size_t>(frame.moving.size(), gamemodels::kCardMostMoving);
            modelMatrices_ = 0;
            if (moving && frame.matrices.size() >= moving * 12) {
                modelMatrices_ = modelFacesRoom_ * (kFaceWords / 4);
                const D3D11_BOX box{modelMatrices_ * 16, 0, 0, modelMatrices_ * 16 + uint32_t(moving) * 48, 1, 1};
                ctx_->UpdateSubresource(modelFaces_, 0, &box, frame.matrices.data(), 0, 0);
            }
            // 3.0.20-beta.34: the matrices kept for the leaves after, and their camera (leaf_vs)
            modelMatricesKept_ = modelMatrices_;
            modelCameraAt_ = 0;
            if (leavesFromCard11()) {
                modelCameraAt_ = modelFacesRoom_ * (kFaceWords / 4) + gamemodels::kCardMostMoving * 3;
                const D3D11_BOX box{modelCameraAt_ * 16, 0, 0, modelCameraAt_ * 16 + 48, 1, 1};
                ctx_->UpdateSubresource(modelFaces_, 0, &box, viewToWorld_, 0, 0);
            }
            static const std::vector<uint32_t> kNoMoving;
            // the frame's faces by texture, the textures first met
            // 3.0.20-beta.37 (as game_models.cpp): many nodes - gathered in parts on the frame's threads
            const std::vector<uint32_t>& movingNow = modelMatrices_ ? frame.moving : kNoMoving;
            if (frame.ids.size() + movingNow.size() >= kPartsFrom) {
                if (modelParts_.size() < kFrameParts) modelParts_.resize(kFrameParts);
                parallelChunks(kFrameParts, [&](size_t k) {
                    modelCards_.framePart(frame.ids, movingNow, k, kFrameParts, modelParts_[k]);
                });
                gamemodels::joinParts(modelParts_, kFrameParts, modelFrame_);
            } else {
                modelCards_.frameOf(frame.ids, movingNow, modelFrame_);
            }
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
            bool castersNoted = false;
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
                castersNoted = true;
                ++modelsDrawnFrames_;
                modelsDrawnFaces_ += modelInstanceData_.size();
                modelsDrawnNodes_ += nodes;
                modelsUnknown_ += unknown;
                modelsDrawnMoving_ += modelFrame_.moving;  // 3.0.20-beta.14
            }
            // 3.0.20-beta.40: the card's leaves' casters when no other face of the frame was drawn (a picture of
            // leaves alone left them out)
            if (!castersNoted && leavesOnCardNow11()) noteModelCasters();
            modelMatrices_ = 0;
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
    if (!leavesOwnWay() || frame.empty() || !modelLikeKnown_ || !cameraValid_) return;
    if (leavesOnCardNow11()) {  // 3.0.20-beta.34: from the card, the camera put up with the models'
        drawGameLeavesCard11();
        return;
    }
    gamemodels::Picture pic;
    if (!own_ground::viewOf(viewToWorld_, pic.view)) return;
    pic.p = camera::projectionFor(gameWidth_, gameHeight_);
    pic.how = gamemodels::colouringOf(frame);
    modelFog_.fit();
    pic.fog = modelFog_.fogged() ? &modelFog_ : nullptr;
    pic.plain = modelFog_.specularAt(0.0f);
    // 3.0.20-beta.16 (R-GZDPF9): the nodes with leaves placed a node each on the own ground's threads
    // (core gamemodels::Leaves), their draws after in the frame's order
    leaves11_.clear();
    const auto leafy = [this](uint32_t texture) { return leafyTexture(texture); };
    for (size_t i = 0; i < frame.ids.size() + frame.moving.size(); ++i) {  // 3.0.20-beta.14: the moving ones too
        const float* toView = nullptr;
        if (const gamemodels::Node* n = modelNodeOf(frame, i, toView)) leaves11_.add(*n, toView, leafy);
    }
    if (!leaves11_.size()) return;
    leaves11_.place(pic, parallelChunks);
    leafRec_.reset();
    DrawSetup st = modelLike_;
    st.cull = d3d7::CULL_NONE;  // culled by place() as the game culls
    const uint32_t state = leafRec_.addState(st);
    for (size_t k = 0; k < leaves11_.size(); ++k) {
        const gamemodels::Placed& out = leaves11_.placed(k);
        for (size_t t = 0; t < out.textures(); ++t) {
            const std::vector<own_ground::Tl1>& v = out.vertices(t);
            if (v.empty() || v.size() > 65535) continue;
            DrawCmd cmd;
            cmd.state = state;
            cmd.texture[0] = out.texture(t);
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

// 3.0.20-beta.34 (the owner's «6,7,8,9», 8 - as Direct3D 12's drawGameLeavesCard): a tree's leafy runs from the
// card a draw each (leaf_vs, their faces' numbers as instances), their sway and crown's look from the tree's box
// (cardTreeLook); their casters the card's (noteModelCasters)
void Renderer11::drawGameLeavesCard11() {
    const gamemodels::Frame& frame = modelsNow_.frame();
    own_ground::View view;
    if (!own_ground::viewOf(viewToWorld_, view)) return;
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    const LeafLook look = leafLookNow();
    const unsigned long long now = GetTickCount64();
    if (!clockStart_) clockStart_ = now;
    const float time = float(double(now - clockStart_) / 1000.0);
    const float swing = effects_.foliage && effects_.foliageStrength > 0
                            ? foliage::kSwing * float(effects_.foliageStrength) / 100.0f
                            : 0.0f;
    leafCardRec_.reset();
    leafCardDraws_.clear();
    leafCardData_.clear();
    leafCardLooks_.clear();
    DrawSetup st = modelLike_;
    st.cull = d3d7::CULL_NONE;  // culled in leaf_vs as the game culls
    const uint32_t state = leafCardRec_.addState(st);
    static const uint8_t kNone[3 * 32] = {};  // the record's draw: three vertices of nothing (leaf_vs reads)
    const auto leafy = [this](uint32_t texture) { return leafyTexture(texture); };
    // 3.0.20-beta.79 ([view3d] crown_light, as Direct3D 12's): the still crowns lit by their shape
    float sunWay[3] = {0.0f, -1.0f, 0.0f};
    if (effects_.crownLight) mapSun(sunWay);
    const float* crownSun = effects_.crownLight && camera::wideView() ? sunWay : nullptr;
    const auto tree = [&](uint32_t id, uint32_t bits, const float* toView) {
        const std::vector<gamemodels::CardRun>* runs = modelCards_.runsOfNode(id);
        // 3.0.20-beta.40: a still one listed as moving or the other way round - left out, as frameOf leaves it
        if (!runs || modelCards_.moving(id) != (toView != nullptr)) return;
        Constants leaf{};
        critters::Crown crown;
        bool crownSeen = false;
        if (!cardTreeLook(*runs, leafy, toView, view, p, viewToWorld_, look, swing, time, leaf,
                          effects_.leafFall && frameCrowns_.size() < 32 ? &crown : nullptr, &crownSeen, crownSun))
            return;
        if (crownSeen) frameCrowns_.push_back(crown);
        // 3.0.20-beta.51 ([view3d] dense_crowns, as Direct3D 12's): a still tree's leaves twice more, turned
        const bool dense = effects_.denseCrowns && camera::wideView() && !toView;
        // 3.0.20-beta.73 ([view3d] crown_volume, as Direct3D 12's): each leaf face once more, stood up
        const bool volume = effects_.crownVolume && camera::wideView() && !toView;
        for (const gamemodels::CardRun& r : *runs) {
            if (!r.count || !leafyTexture(r.texture)) continue;
            leafCardDraws_.push_back(ModelDraw11{uint32_t(leafCardData_.size()), r.count});
            for (uint32_t f = 0; f < r.count; ++f) leafCardData_.push_back((r.first + f) | bits);
            DrawCmd cmd;
            cmd.state = state;
            cmd.texture[0] = r.texture;
            cmd.primitive = d3d7::PT_TRIANGLELIST;
            cmd.fvf = d3d7::FVF_TL1;
            leafCardRec_.draw(cmd, kNone, 3, nullptr, 0);
            leafCardLooks_.push_back(leaf);
            for (int copy = 0; dense && copy < foliage::kCrownCopies; ++copy) {
                Constants turned = leaf;
                foliage::crownTurn(copy, turned.reproject + 9);
                const ModelDraw11 same = leafCardDraws_.back();  // the same faces again
                leafCardDraws_.push_back(same);
                leafCardRec_.draw(cmd, kNone, 3, nullptr, 0);
                leafCardLooks_.push_back(turned);
            }
            for (int copy = 0; volume && copy < foliage::kCrownStood; ++copy) {
                Constants stood = leaf;
                foliage::crownStood(copy, stood.reproject + 9);
                const ModelDraw11 same = leafCardDraws_.back();  // the same faces again
                leafCardDraws_.push_back(same);
                leafCardRec_.draw(cmd, kNone, 3, nullptr, 0);
                leafCardLooks_.push_back(stood);
            }
        }
    };
    for (const uint32_t id : frame.ids) tree(id, 0u, nullptr);
    if (modelMatricesKept_ && frame.matrices.size() >= frame.moving.size() * 12)
        for (size_t m = 0; m < frame.moving.size() && m < gamemodels::kCardMostMoving; ++m)
            tree(frame.moving[m], uint32_t(m + 1) << gamemodels::kCardSlotShift, frame.matrices.data() + m * 12);
    if (leafCardDraws_.empty()) return;
    modelFog_.fit();  // as the models' pass (none of theirs drawn when all their faces are leaves)
    static bool told = false;
    if (!told) {
        told = true;
        LOG_INFO("game models: their leaves drawn from the card (leaf_vs, Direct3D 11) - %zu draws in the first "
                 "picture, their sway and crowns' look from their boxes",
                 leafCardDraws_.size());
    }
    flushBatch();  // the game's draws gathered so far
    if (!fillBuffer(modelInstances_, modelInstanceBytes_, D3D11_BIND_VERTEX_BUFFER, leafCardData_.data(),
                    leafCardData_.size() * sizeof(uint32_t), nullptr))
        return;
    modelPass_ = leafPass_ = true;
    for (size_t n = 0; n < leafCardDraws_.size(); ++n) {
        modelDraw_ = leafCardDraws_[n];
        leafLook_ = leafCardLooks_[n];
        draw(leafCardRec_, leafCardRec_.draws()[n]);
    }
    flushBatch();
    modelDraw_ = ModelDraw11{};
    modelPass_ = leafPass_ = false;
}

LeafLook Renderer11::leafLookNow() const {
    LeafLook l;
    l.volume = effects_.treeVolume;
    l.through = effects_.leafLight;
    l.tints = effects_.treeTints;
    const daylight::Light sun = daylight::at(effects_.dayCycle ? effects_.dayNow : -1);
    l.sun[0] = sun.shadow[0];
    l.sun[1] = sun.shadow[1];
    l.throughLight = trees::throughLight(sun.length, float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f);
    l.season = season_;
    l.viewToWorld = cameraValid_ ? viewToWorld_ : nullptr;
    l.projection = camera::projectionFor(gameWidth_, gameHeight_);
    return l;
}

}  // namespace uagfx::gpu
