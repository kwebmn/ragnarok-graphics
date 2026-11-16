#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "core/foliage.hpp"
#include "core/game_models_place.hpp"
#include "core/model_census.hpp"
#include "win/gpu/draw_constants.hpp"
#include "win/gpu/impl.hpp"
#include "win/gpu/threads.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 3.0.20-beta.3 (stage 2a of «Модели нашим рендером»): a node the driver captured - kept by its node; a new map's
// first one lets the last map's go
void Renderer::Impl::addGameModel(const gamemodels::Node& n) {
    if (n.map != gameModelsMap) {
        gameCards.clear();  // 3.0.20-beta.11
        gameFacesUp = 0;
        gameModels.clear();
        gameModelFaces = gameModelCorners = gameModelCornersOurs = gameModelsTold = 0;
        ownCorners.clear();
        ownCornersMade = false;
        gameModelsUnchecked.clear();
        gameModelsMap = n.map;
    }
    const auto [it, fresh] = gameModels.insert_or_assign(n.id, n);
    if (!fresh) return;  // the same node again: the first one kept its count
    gameCards.add(n);  // 3.0.20-beta.11: in the card's form too
    gameModelFaces += n.faces.size();
    if (makeOwnCorners())
        checkGameModel(it->second);
    else
        gameModelsUnchecked.push_back(n.id);
}

// our own models' corners (placed from the map's files, in the game's world by the map's axes) - once they are up
bool Renderer::Impl::makeOwnCorners() {
    if (ownCornersMade) return true;
    const OwnGround* g = mapWorld.ownGround();
    if (!g || g->models.vertices.empty() || !mapWorld.axes().known()) return false;
    const map::Axes a = mapWorld.axes();
    for (const terrain::Vertex& v : g->models.vertices) {
        const float w[3] = {float(a.x) * v.pos[0], float(a.y) * v.pos[1], float(a.z) * v.pos[2]};
        ownCorners.add(w);
    }
    ownCornersMade = true;
    for (const uint32_t id : gameModelsUnchecked)
        if (const auto it = gameModels.find(id); it != gameModels.end()) checkGameModel(it->second);
    gameModelsUnchecked.clear();
    return true;
}

void Renderer::Impl::checkGameModel(const gamemodels::Node& n) {
    if (n.moving) return;  // 3.0.20-beta.16: its own space, not the world's places of our models
    for (size_t i = 0; i + 2 < n.vertices.size(); i += 3) {
        ++gameModelCorners;
        if (ownCorners.within(&n.vertices[i], 0.5f)) ++gameModelCornersOurs;
    }
}

// 3.0.20-beta.4: the frame's game models, hashed (FNV-1a) - the own ground's record is made again when they change
uint64_t Renderer::Impl::gameFrameKey() const {
    const gamemodels::Frame& f = gameModelsNow.frame();
    if (f.empty()) return 0;
    uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
    };
    mix(f.ids.data(), f.ids.size() * sizeof(uint32_t));
    mix(f.moving.data(), f.moving.size() * sizeof(uint32_t));  // 3.0.20-beta.14: the moving ones by their matrices
    mix(f.matrices.data(), f.matrices.size() * sizeof(float));
    mix(f.factors, sizeof f.factors);
    const uint8_t lit = f.lit ? 1 : 0;
    mix(&lit, 1);
    return h | 1;
}

// 3.0.20-beta.4 (stage 2b of «Модели нашим рендером»): the game's still models the driver left out of its draw this
// frame (gameModelsNow's list), from their captured nodes - each corner placed as the own ground's, coloured as the
// game colours it (core/model_census expected: its lit ints or colour bytes x the factors), its texture the game's
// own; with the game's model state (both sides drawn, as the models past the far plane), into the own ground's record
// (layer 3)
void Renderer::Impl::drawGameModels(const own_ground::View& view, const camera::Projection& p, FrameRecord& rec,
                                    std::vector<uint8_t>* layers, const own_ground::Shading& fog) {
    OwnState& o = ownState;
    o.gameTriangles = o.gameNodes = o.gameUnknown = 0;
    gameMovingPlaced = 0;
    const gamemodels::Frame& frame = gameModelsNow.frame();
    const std::vector<uint32_t>& ids = frame.ids;
    if (frame.empty() || !o.modelLikeKnown) return;
    DrawSetup ms = o.modelLike;
    ms.cull = d3d7::CULL_NONE;
    const uint32_t state = rec.addState(ms);
    gamemodels::Picture pic;
    pic.view = view;
    pic.p = p;
    pic.how.way = gameModelsNow.frame().lit ? modelcensus::Way::Lit : modelcensus::Way::Plain;
    std::copy(gameModelsNow.frame().factors, gameModelsNow.frame().factors + 3, pic.how.factors);
    pic.how.alpha = 0xFF;
    pic.fog = fog.fogged() ? &fog : nullptr;
    pic.plain = fog.specularAt(0.0f);
    // 3.0.20-beta.8 (the owner's «давай по очереди», step 1 - R-XTEFYA: «the own ground» +2 ms a frame): the list's
    // nodes in turns of kGameNodesAJob, each turn placed on a thread of its own (parallelChunks, as the own ground's
    // chunks; core/game_models_place - each vertex once a node), then put into the draws in the list's order: the same
    // draws as one thread made
    // 3.0.20-beta.14 (stage 4): the frame's moving nodes after its still ones, each put into the view by its matrix
    const size_t all = ids.size() + frame.moving.size();
    const size_t jobs = (all + kGameNodesAJob - 1) / kGameNodesAJob;
    if (gameOuts.size() < jobs) gameOuts.resize(jobs);
    if (gameScratch.size() < jobs) gameScratch.resize(jobs);
    parallelChunks(jobs, [&](size_t job) {
        gamemodels::Placed& out = gameOuts[job];
        out.clear();
        const size_t end = std::min(all, (job + 1) * kGameNodesAJob);
        for (size_t i = job * kGameNodesAJob; i < end; ++i) {
            const float* toView = nullptr;
            const gamemodels::Node* n = gameNodeOf(frame, i, toView);
            if (!n)
                ++out.unknown;  // 3.0.20-beta.7
            else
                gamemodels::place(*n, pic, gameScratch[job], out, nullptr, toView);
        }
    });
    for (size_t k = 0; k < frame.moving.size(); ++k) {
        const float* toView = nullptr;
        if (gameNodeOf(frame, ids.size() + k, toView)) ++gameMovingPlaced;
    }
    for (auto& [id, pd] : gamePending) pd.verts.clear(), pd.idx.clear();
    const auto emit = [&](uint32_t texture) {
        OwnState::ModelPending& pd = gamePending[texture];
        if (!pd.idx.empty()) {
            DrawCmd cmd;
            cmd.state = state;
            cmd.texture[0] = texture;
            cmd.primitive = d3d7::PT_TRIANGLELIST;
            cmd.fvf = d3d7::FVF_TL1;
            rec.draw(cmd, pd.verts.data(), uint32_t(pd.verts.size()), pd.idx.data(), uint32_t(pd.idx.size()));
            if (layers) layers->push_back(3);
        }
        pd.verts.clear();
        pd.idx.clear();
    };
    for (size_t job = 0; job < jobs; ++job) {
        const gamemodels::Placed& out = gameOuts[job];
        o.gameTriangles += out.triangles;
        o.gameNodes += out.nodes;
        o.gameUnknown += out.unknown;
        for (size_t t = 0; t < out.textures(); ++t) {
            const uint32_t texture = out.texture(t);
            if (leavesOwnWay() && leafyTexture(texture)) continue;  // 3.0.20-beta.13: drawGameLeaves draws them
            const std::vector<own_ground::Tl1>& v = out.vertices(t);
            for (size_t at = 0; at < v.size();) {
                OwnState::ModelPending& pd = gamePending[texture];
                const size_t room = (65535 - pd.verts.size()) / 3 * 3;  // a draw's most: 21845 faces
                if (!room) {
                    emit(texture);
                    continue;
                }
                const size_t n = std::min(room, v.size() - at);
                const uint16_t base = uint16_t(pd.verts.size());
                pd.verts.insert(pd.verts.end(), v.begin() + ptrdiff_t(at), v.begin() + ptrdiff_t(at + n));
                for (size_t k = 0; k < n; ++k) pd.idx.push_back(uint16_t(base + k));
                at += n;
            }
        }
    }
    for (auto& [texture, pd] : gamePending) emit(texture);
}

// 3.0.20-beta.14 (stage 4): the frame's node `i` - its still ones, then its moving ones (`toView` its matrix this
// frame); none when unknown, or a still one listed as moving or the other way round
const gamemodels::Node* Renderer::Impl::gameNodeOf(const gamemodels::Frame& frame, size_t i,
                                                   const float*& toView) const {
    const bool still = i < frame.ids.size();
    const size_t k = still ? 0 : i - frame.ids.size();
    toView = nullptr;
    if (!still && (k >= frame.moving.size() || frame.matrices.size() < 12 * (k + 1))) return nullptr;
    const auto it = gameModels.find(still ? frame.ids[i] : frame.moving[k]);
    if (it == gameModels.end() || it->second.moving == still) return nullptr;
    if (!still) toView = &frame.matrices[12 * k];
    return &it->second;
}

// 3.0.20-beta.18 (the owner's «ок», item 2 - R-DHG86V 15 %, R-8UPTJW 3 % of the noted triangles where ours lie: with
// «Моделі нашим рендером» the game draws almost none of its models, and the far models learnt their colours from little
// else): the frame's nodes we drew noted as the game's draws of them were (noteOwnModels) - their corners in the file's
// places, the colours the game gives them - in the same turns (own_models::NotingTurn); the leaves drawn the world's
// way are noted there already
void Renderer::Impl::noteGameModelsForFar() {
    OwnState& o = ownState;
    const OwnGround* g = mapWorld.ownGround();
    const gamemodels::Frame& frame = gameModelsNow.frame();
    if (!g || g->serial != o.serial || !effects.ownFarModels || frame.empty() || !viewToWorldValid) return;
    own_models::NotingTurn& turn = o.modelsTurn;
    if (turn.noted >= own_models::kMostNoted) return;
    const map::Axes axes = mapWorld.axes().known() ? mapWorld.axes() : map::Axes{1, 1, 1};
    const float sign[3] = {float(axes.x), float(axes.y), float(axes.z)};
    const modelcensus::Colouring how = gamemodels::colouringOf(frame);
    const bool leavesApart = leavesOwnWay();
    for (size_t i = 0; i < frame.ids.size() + frame.moving.size() && turn.noted < own_models::kMostNoted; ++i) {
        const float* toView = nullptr;
        const gamemodels::Node* n = gameNodeOf(frame, i, toView);
        if (!n) continue;
        const uint32_t faces = uint32_t(n->faces.size());
        if (turn.passed + faces <= turn.from) {  // before this picture's turn
            turn.passed += faces;
            continue;
        }
        float toWorld[12];
        if (toView) gamemodels::compose(toView, viewToWorld, toWorld);
        for (const gamemodels::Face& f : n->faces) {
            if (turn.noted >= own_models::kMostNoted) break;
            if (turn.passed++ < turn.from) continue;
            const uint32_t texture = f.texture < n->textures.size() ? n->textures[f.texture] : 0;
            // 3.0.20-beta.40: the leaves from the card are no draws of the world's way - noted here as the rest
            if (!texture || (leavesApart && !leavesOnCardNow() && leafyTexture(texture))) continue;
            float corners[3][3];
            uint32_t colours[3];
            gamemodels::faceForLearning(*n, f, toView ? toWorld : nullptr, sign, how, corners, colours);
            o.learning.note(corners, colours);
            ++turn.noted;
        }
    }
}

// 3.0.20-beta.9: as the own ground's draws (drawOwnGround) - after the game's draws gathered so far, the own pass
// (no sway of leaves - a draw holds many trees -, not a caster)
bool Renderer::Impl::drawGameModelsAlone() {
    gameAloneDrawn = true;
    OwnState& o = ownState;
    own_ground::View view;
    const bool ready = o.modelLikeKnown && viewToWorldValid && own_ground::viewOf(viewToWorld, view);
    if (ready && !gameModelsNow.frame().empty() && cardModelsReady()) {  // 3.0.20-beta.11: from the card
        gameFog.fit();
        drawGameModelsCard(gameFog);
    } else if (ready && !gameModelsNow.frame().empty()) {
        const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
        gameFog.fit();
        gameRec.reset();
        drawGameModels(view, p, gameRec, nullptr, gameFog);
        if (!gameRec.draws().empty()) {
            flushDraws();  // the game's draws gathered so far are of its own vertices, uploaded before these
            prepareGeometry(gameRec);
            if (!vertexBase) return true;
            ownPass = true;
            for (const DrawCmd& d : gameRec.draws()) draw(gameRec, d);
            ownPass = false;
            geometryList = 0;  // the game's record's vertices go up again with its next draw
        }
        noteGameCasters();  // 3.0.20-beta.12
        ++gameDrawnFrames;  // 3.0.20-beta.11: for the log
        gameDrawnTriangles += o.gameTriangles;
        gameDrawnNodes += o.gameNodes;
        gameDrawnUnknown += o.gameUnknown;
        gameDrawnMoving += gameMovingPlaced;  // 3.0.20-beta.14
    }
    if (ready) {  // 3.0.20-beta.13 (3.0.20-beta.16: their own time)
        CpuTimeWithin t(cpuPartTicks[kCpuGameLeaves], cpuPartTicks[kCpuGameModels]);
        drawGameLeaves(gameFog);
    }
    gameModelsNow.drawn(ready);  // the driver's word: from the present on
    return true;
}

// 3.0.20-beta.9: the fog of the game's world draws this picture - a few vertices of each (its specular's alpha by the
// view depth), the line fitted when our models are drawn (Shading::fit)
void Renderer::Impl::noteGameFog(const FrameRecord& rec, const DrawCmd& d) {
    constexpr uint32_t kMostAPicture = 512, kADraw = 4;
    if (gameFogVertices >= kMostAPicture || !d.vertexCount) return;
    const uint32_t stride = FrameRecord::stride(d.fvf);
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    if (stride < 24 || d.vertexOffset + size_t(d.vertexCount) * stride > vb.size()) return;
    const uint32_t step = std::max<uint32_t>(1, d.vertexCount / kADraw);
    for (uint32_t i = 0; i < d.vertexCount && gameFogVertices < kMostAPicture; i += step) {
        float rhw;
        uint32_t specular;
        std::memcpy(&rhw, vb.data() + d.vertexOffset + size_t(i) * stride + 12, 4);
        std::memcpy(&specular, vb.data() + d.vertexOffset + size_t(i) * stride + 20, 4);
        if (rhw <= 0.0f) continue;
        gameFog.fog(1.0f / rhw, specular);
        ++gameFogVertices;
    }
}

// 3.0.20-beta.11: room in the card's buffer of faces for `faces` - an upload heap's (the card reads it where it lies,
// the processor writes into it mapped), grown twice at a time; the last one let go once the card is done with it
bool Renderer::Impl::ensureGameFaces(uint32_t faces) {
    if (gameFaces && gameFacesRoom >= faces) return true;
    uint32_t room = std::max<uint32_t>(gameFacesRoom, 4096);
    while (room < faces) room *= 2;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    // 3.0.20-beta.14 (stage 4): after the faces, the frame's moving nodes' matrices - a region a slot (the list in
    // flight before this one may still read its own), kCardMostMoving of three uint4 each
    // 3.0.20-beta.33: and a camera a slot (the leaves', leaf_vs)
    desc.Width = UINT64(room) * gamemodels::kCardFaceWords * 4 + UINT64(kSlots) * gamemodels::kCardMostMoving * 48 +
                 UINT64(kSlots) * 48;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* made = nullptr;
    HRESULT hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 nullptr, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&made));
    void* mapped = nullptr;
    if (SUCCEEDED(hr)) {
        const D3D12_RANGE none{0, 0};
        hr = made->Map(0, &none, &mapped);
    }
    if (gameFacesSrv == kNoSrv) gameFacesSrv = allocSrv();
    if (FAILED(hr) || !mapped || gameFacesSrv == kNoSrv) {
        LOG_ERROR("game models: no buffer on the card for %u faces (0x%08lX) - placed by the processor from now on",
                  room, static_cast<unsigned long>(hr));
        if (made) made->Release();
        gameFacesFailed = true;
        return false;
    }
    if (gameFaces) {  // the card may still read the old one: let go with its list
        gameFaces->Unmap(0, nullptr);
        discard(gameFaces);
    }
    gameFaces = made;
    gameFacesMapped = static_cast<uint8_t*>(mapped);
    gameFacesRoom = room;
    gameFacesUp = 0;
    D3D12_SHADER_RESOURCE_VIEW_DESC v{};
    v.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    v.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    v.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    v.Buffer.NumElements =
        room * (gamemodels::kCardFaceWords / 4) + kSlots * gamemodels::kCardMostMoving * 3 + kSlots * 3;
    device->CreateShaderResourceView(gameFaces, &v, cpuSrvHandle(gameFacesSrv));
    static bool told = false;
    if (!told) {
        told = true;
        LOG_INFO("game models: drawn from the card - their faces in a buffer of %u (model_vs)", room);
    }
    return true;
}

// 3.0.20-beta.11 (the owner's «делай все сразу», step 2): the frame's game models from the card - their colours made
// again when the game's factors changed, the faces not there yet written, then a draw a texture: its faces' numbers in
// the record's vertices (eight a vertex of 32 bytes), drawn by model_vs an instance a face, with the game's model
// state, as the own ground's draws (the own pass: no leaves' sway, not a caster)
void Renderer::Impl::drawGameModelsCard(const own_ground::Shading& fog) {
    OwnState& o = ownState;
    const gamemodels::Frame& frame = gameModelsNow.frame();
    o.gameTriangles = o.gameNodes = o.gameUnknown = 0;
    gameMatricesAt = gameCameraAt = 0;  // 3.0.20-beta.34: none this frame till put up below
    if (frame.empty() || !o.modelLikeKnown || !viewToWorldValid) return;
    const uint64_t key = gamemodels::colouringKey(frame);
    if (key != gameCards.colouredWith()) {
        gameCards.colour(gamemodels::colouringOf(frame), key);
        gameFacesUp = 0;
    }
    const uint32_t faces = gameCards.faces();
    if (!faces || !ensureGameFaces(faces)) return;
    if (gameFacesUp < faces) {
        const size_t from = size_t(gameFacesUp) * gamemodels::kCardFaceWords;
        std::memcpy(gameFacesMapped + from * 4, gameCards.words().data() + from,
                    (size_t(faces) * gamemodels::kCardFaceWords - from) * 4);
        gameFacesUp = faces;
    }
    // 3.0.20-beta.14 (stage 4): the frame's moving nodes' matrices into this list's region after the faces (holeG: its
    // first uint4), their faces into the batches with their places in the list - model_vs puts them by those
    cardMatrices = 0;
    const size_t moving = open ? std::min<size_t>(frame.moving.size(), gamemodels::kCardMostMoving) : 0;
    if (moving && frame.matrices.size() >= moving * 12) {
        cardMatrices = gameFacesRoom * (gamemodels::kCardFaceWords / 4) + slot * gamemodels::kCardMostMoving * 3;
        std::memcpy(gameFacesMapped + size_t(cardMatrices) * 16, frame.matrices.data(), moving * 48);
    }
    // 3.0.20-beta.33: the matrices kept for the leaves after, and their camera (leaf_vs) in this slot's own place
    gameMatricesAt = cardMatrices;
    gameCameraAt = 0;
    if (open && leavesFromCard()) {
        gameCameraAt = gameFacesRoom * (gamemodels::kCardFaceWords / 4) + kSlots * gamemodels::kCardMostMoving * 3 +
                       slot * 3;
        std::memcpy(gameFacesMapped + size_t(gameCameraAt) * 16, viewToWorld, 48);
    }
    static const std::vector<uint32_t> kNoMoving;
    // 3.0.20-beta.37 (the owner's «6,7,8,9», 9): many nodes - gathered in parts on the frame's threads, put together
    // in their order (core gamemodels::joinParts: the same as at once)
    const std::vector<uint32_t>& movingNow = cardMatrices ? frame.moving : kNoMoving;
    if (frame.ids.size() + movingNow.size() >= kPartsFrom) {
        if (cardParts.size() < kFrameParts) cardParts.resize(kFrameParts);
        parallelChunks(kFrameParts, [&](size_t k) {
            gameCards.framePart(frame.ids, movingNow, k, kFrameParts, cardParts[k]);
        });
        gamemodels::joinParts(cardParts, kFrameParts, gameCardFrame);
    } else {
        gameCards.frameOf(frame.ids, movingNow, gameCardFrame);
    }
    cardRec.reset();
    cardCounts.clear();
    DrawSetup st = o.modelLike;
    st.cull = d3d7::CULL_NONE;  // culled in model_vs as the game culls
    const uint32_t state = cardRec.addState(st);
    const bool leavesApart = leavesOwnWay();  // 3.0.20-beta.13: drawn as the game's trees (drawGameLeaves)
    for (size_t t = 0; t < gameCardFrame.textures.size(); ++t) {
        const std::vector<uint32_t>& list = gameCardFrame.faces[t];
        if (list.empty() || (leavesApart && leafyTexture(gameCardFrame.textures[t]))) continue;
        cardPacked.assign(list.begin(), list.end());
        // 3.0.20-beta.40 (the audit): never four vertices - draw() takes four for a sprite's quad (ai::flatQuad of the
        // faces' numbers read as places said yes): 25 to 32 faces of a texture in a picture went without their
        // material and into DLSS's mask, a picture at a time as the camera turned
        size_t packed = (list.size() + 7) / 8;
        if (packed == 4) packed = 5;
        cardPacked.resize(packed * 8, 0u);
        DrawCmd cmd;
        cmd.state = state;
        cmd.texture[0] = gameCardFrame.textures[t];
        cmd.primitive = d3d7::PT_TRIANGLELIST;
        cmd.fvf = d3d7::FVF_TL1;
        cardRec.draw(cmd, cardPacked.data(), uint32_t(packed), nullptr, 0);
        cardCounts.push_back(UINT(list.size()));
        o.gameTriangles += list.size();
    }
    o.gameNodes = gameCardFrame.nodes;
    o.gameUnknown = gameCardFrame.unknown;
    if (cardCounts.empty()) {
        cardMatrices = 0;
        noteGameCasters();  // 3.0.20-beta.40: the card's leaves' (a picture of leaves alone left them out)
        return;
    }
    flushDraws();  // the draws gathered so far are of the record's vertices before these
    prepareGeometry(cardRec);
    if (vertexBase) {
        cardFog = &fog;
        ownPass = cardModelsDraw = true;
        for (size_t n = 0; n < cardCounts.size(); ++n) {
            cardCount = cardCounts[n];
            draw(cardRec, cardRec.draws()[n]);
        }
        flushDraws();
        ownPass = cardModelsDraw = false;
        cardCount = 0;
        cardFog = nullptr;
    }
    cardMatrices = 0;
    geometryList = 0;  // the game's record's vertices go up again with its next draw
    noteGameCasters();  // 3.0.20-beta.12: the reflections and the torches' shadows take them
    ++gameDrawnFrames;
    ++gameDrawnByCard;
    gameDrawnTriangles += o.gameTriangles;
    gameDrawnNodes += o.gameNodes;
    gameDrawnUnknown += o.gameUnknown;
    gameDrawnMoving += gameCardFrame.moving;  // 3.0.20-beta.14
    checkGameModelsShown(fog);  // 3.0.20-beta.18
}

void Renderer::Impl::checkGameModelsShown(const own_ground::Shading& fog) {
    const gamemodels::Frame& frame = gameModelsNow.frame();
    const size_t all = frame.ids.size() + frame.moving.size();
    gamemodels::Picture pic;
    if (!all || !own_ground::viewOf(viewToWorld, pic.view)) return;
    pic.p = camera::projectionFor(gameWidth, gameHeight);
    pic.how = gamemodels::colouringOf(frame);
    pic.fog = fog.fogged() ? &fog : nullptr;
    pic.plain = fog.specularAt(0.0f);
    constexpr int kAPicture = 4;
    for (int k = 0; k < kAPicture; ++k) {
        const size_t i = shownCheckAt++ % all;
        const float* toView = nullptr;
        const gamemodels::Node* n = gameNodeOf(frame, i, toView);
        if (!n || n->faces.empty()) continue;
        shownOut.clear();
        gamemodels::place(*n, pic, shownScratch, shownOut, nullptr, toView);
        size_t before = 0;  // its corners in front of the eye
        for (size_t v = 0; v < n->vertices.size() / 3 && v < shownScratch.size(); ++v) before += shownScratch[v].ok;
        if (!before) continue;
        ++shownChecked;
        if (shownOut.triangles) continue;
        ++shownNone;
        if (shownExamples.size() >= 4) continue;
        float at[3] = {n->vertices[0], n->vertices[1], n->vertices[2]};  // its first corner in the world
        if (toView) {
            float toWorld[12];
            gamemodels::compose(toView, viewToWorld, toWorld);
            gamemodels::transform(toWorld, &n->vertices[0], at);
        }
        char example[160];
        std::snprintf(example, sizeof example, "x %.0f z %.0f y %.0f - %zu faces, %zu corners in front%s%s", at[0],
                      at[2], at[1], n->faces.size(), before, n->flipped ? ", mirrored" : "",
                      n->moving ? ", moving" : "");
        shownExamples.push_back(example);
    }
}

// 3.0.20-beta.13 (the owner: «не качаются деревья»): the frame's game models' leaves drawn as the game's own trees - a
// draw a node's leafy texture (as its own draw items), placed by the processor, coloured as the game, through the
// world's path: the wind's sway by the draw's extent, the crowns' volume, the light through, the tints by its place,
// its casters noted as the game's
void Renderer::Impl::drawGameLeaves(const own_ground::Shading& fog) {
    const gamemodels::Frame& frame = gameModelsNow.frame();
    if (!leavesOwnWay() || frame.empty() || !ownState.modelLikeKnown || !viewToWorldValid) return;
    if (leavesOnCardNow()) {  // 3.0.20-beta.33: from the card, the camera put up with the models'
        drawGameLeavesCard(fog);
        return;
    }
    gamemodels::Picture pic;
    if (!own_ground::viewOf(viewToWorld, pic.view)) return;
    pic.p = camera::projectionFor(gameWidth, gameHeight);
    pic.how = gamemodels::colouringOf(frame);
    pic.fog = fog.fogged() ? &fog : nullptr;
    pic.plain = fog.specularAt(0.0f);
    // 3.0.20-beta.16 (R-GZDPF9): the nodes with leaves placed a node each on the own ground's threads
    // (core gamemodels::Leaves), their draws after in the frame's order
    gameLeaves.clear();
    const auto leafy = [this](uint32_t texture) { return leafyTexture(texture); };
    for (size_t i = 0; i < frame.ids.size() + frame.moving.size(); ++i) {  // 3.0.20-beta.14: the moving ones too
        const float* toView = nullptr;
        if (const gamemodels::Node* n = gameNodeOf(frame, i, toView)) gameLeaves.add(*n, toView, leafy);
    }
    if (!gameLeaves.size()) return;
    gameLeaves.place(pic, parallelChunks);
    leafRec.reset();
    DrawSetup st = ownState.modelLike;
    st.cull = d3d7::CULL_NONE;  // culled by place() as the game culls
    const uint32_t state = leafRec.addState(st);
    for (size_t k = 0; k < gameLeaves.size(); ++k) {
        const gamemodels::Placed& out = gameLeaves.placed(k);
        for (size_t t = 0; t < out.textures(); ++t) {
            const std::vector<own_ground::Tl1>& v = out.vertices(t);
            if (v.empty() || v.size() > 65535) continue;
            DrawCmd cmd;
            cmd.state = state;
            cmd.texture[0] = out.texture(t);
            cmd.primitive = d3d7::PT_TRIANGLELIST;
            cmd.fvf = d3d7::FVF_TL1;
            leafRec.draw(cmd, v.data(), uint32_t(v.size()), nullptr, 0);
        }
    }
    if (leafRec.draws().empty()) return;
    flushDraws();  // the draws gathered so far are of the record's vertices before these
    prepareGeometry(leafRec);
    if (vertexBase)
        for (const DrawCmd& d : leafRec.draws()) draw(leafRec, d);  // the world's path, as the game's trees
    flushDraws();
    geometryList = 0;  // the game's record's vertices go up again with its next draw
}

// 3.0.20-beta.33 (the owner's «6,7,8,9», 8 - the leaves on the card too: 0.24-0.54 ms of the owner's frame placing
// them on the processor, a tree a draw): a tree's leafy runs from the card a draw each (leaf_vs: their faces' numbers
// as instances, a moving node's with its place in the frame's moving list), drawn as the game's trees are - their
// sway and crown's look (leafShadingOf) worked out once a tree from its leafy runs' box, its eight corners onto the
// screen; the camera put up with the models' (gameCameraAt). Their casters are the card's (noteGameCasters)
void Renderer::Impl::drawGameLeavesCard(const own_ground::Shading& fog) {
    const gamemodels::Frame& frame = gameModelsNow.frame();
    own_ground::View view;
    if (!own_ground::viewOf(viewToWorld, view)) return;
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const LeafLook look = leafLook();
    const bool sway = effects.foliage && effects.foliageStrength > 0;
    leafCardRec.reset();
    leafCardCounts.clear();
    leafCardLooks.clear();
    DrawSetup st = ownState.modelLike;
    st.cull = d3d7::CULL_NONE;  // culled in leaf_vs as the game culls
    const uint32_t state = leafCardRec.addState(st);
    const float swing = sway ? foliage::kSwing * float(effects.foliageStrength) / 100.0f : 0.0f;
    const auto leafy = [this](uint32_t texture) { return leafyTexture(texture); };
    // a node's leaves: `bits` its faces' moving place, toView its matrix this frame (node -> view; none: still)
    const auto tree = [&](uint32_t id, uint32_t bits, const float* toView) {
        const std::vector<gamemodels::CardRun>* runs = gameCards.runsOfNode(id);
        // 3.0.20-beta.40: a still one listed as moving or the other way round - left out, as frameOf leaves it
        if (!runs || gameCards.moving(id) != (toView != nullptr)) return;
        Constants leaf{};
        critters::Crown crown;
        bool crownSeen = false;
        if (!cardTreeLook(*runs, leafy, toView, view, p, viewToWorld, look, swing, waterTime(), leaf,
                          effects.leafFall && frameCrowns.size() < 32 ? &crown : nullptr, &crownSeen))
            return;
        if (crownSeen) frameCrowns.push_back(crown);
        // 3.0.20-beta.51 ([view3d] dense_crowns): a still tree's leaves twice more in the 3D view, turned about the
        // crown's middle - the same faces, another look
        const bool dense = effects.denseCrowns && camera::wideView() && !toView;
        for (const gamemodels::CardRun& r : *runs) {
            if (!r.count || !leafyTexture(r.texture)) continue;
            leafCardPacked.clear();
            for (uint32_t f = 0; f < r.count; ++f) leafCardPacked.push_back((r.first + f) | bits);
            uint32_t n = uint32_t((leafCardPacked.size() + 7) / 8);
            if (n == 4) n = 5;  // never a quad's four: draw() takes those for sprites and effects
            leafCardPacked.resize(size_t(n) * 8, 0u);
            DrawCmd cmd;
            cmd.state = state;
            cmd.texture[0] = r.texture;
            cmd.primitive = d3d7::PT_TRIANGLELIST;
            cmd.fvf = d3d7::FVF_TL1;
            leafCardRec.draw(cmd, leafCardPacked.data(), n, nullptr, 0);
            leafCardCounts.push_back(UINT(r.count));
            leafCardLooks.push_back(leaf);
            for (int copy = 0; dense && copy < foliage::kCrownCopies; ++copy) {
                Constants turned = leaf;
                foliage::crownTurn(copy, turned.reproject + 9);
                leafCardRec.draw(cmd, leafCardPacked.data(), n, nullptr, 0);
                leafCardCounts.push_back(UINT(r.count));
                leafCardLooks.push_back(turned);
            }
        }
    };
    for (const uint32_t id : frame.ids) tree(id, 0u, nullptr);
    if (gameMatricesAt && frame.matrices.size() >= frame.moving.size() * 12)
        for (size_t m = 0; m < frame.moving.size() && m < gamemodels::kCardMostMoving; ++m)
            tree(frame.moving[m], uint32_t(m + 1) << gamemodels::kCardSlotShift, frame.matrices.data() + m * 12);
    if (leafCardCounts.empty()) return;
    static bool told = false;
    if (!told) {
        told = true;
        LOG_INFO("game models: their leaves drawn from the card (leaf_vs) - %zu draws in the first picture, their "
                 "sway and crowns' look from their boxes",
                 leafCardCounts.size());
    }
    flushDraws();  // the draws gathered so far are of the record's vertices before these
    prepareGeometry(leafCardRec);
    if (vertexBase) {
        cardFog = &fog;
        ownPass = cardModelsDraw = cardLeavesDraw = true;
        for (size_t n = 0; n < leafCardCounts.size(); ++n) {
            cardCount = leafCardCounts[n];
            cardLeafLook = leafCardLooks[n];
            draw(leafCardRec, leafCardRec.draws()[n]);
        }
        flushDraws();
        ownPass = cardModelsDraw = cardLeavesDraw = false;
        cardCount = 0;
        cardFog = nullptr;
    }
    geometryList = 0;  // the game's record's vertices go up again with its next draw
}

void Renderer::Impl::tellGameModels() {
    if (gameDrawnFrames) {  // 3.0.20-beta.7
        LOG_INFO("game models: drawn by us in %llu pictures (%llu of them from the card) - %.0f triangles a picture, "
                 "%.0f nodes (%.0f of them moving, by their matrices), %.1f of the frame's nodes unknown to the "
                 "renderer",
                 static_cast<unsigned long long>(gameDrawnFrames), static_cast<unsigned long long>(gameDrawnByCard),
                 double(gameDrawnTriangles) / double(gameDrawnFrames), double(gameDrawnNodes) / double(gameDrawnFrames),
                 double(gameDrawnMoving) / double(gameDrawnFrames), double(gameDrawnUnknown) / double(gameDrawnFrames));
        gameDrawnFrames = gameDrawnTriangles = gameDrawnNodes = gameDrawnUnknown = gameDrawnByCard = 0;
        gameDrawnMoving = 0;
    }
    if (shownChecked) {  // 3.0.20-beta.18
        std::string examples;
        for (const std::string& e : shownExamples) examples += (examples.empty() ? "" : "; ") + e;
        LOG_INFO("game models: %llu of %llu nodes checked by the processor showed no face though in front of the "
                 "eye%s%s",
                 static_cast<unsigned long long>(shownNone), static_cast<unsigned long long>(shownChecked),
                 examples.empty() ? "" : " - ", examples.c_str());
        shownChecked = shownNone = 0;
        shownExamples.clear();
    }
    if (gameModels.size() == gameModelsTold) return;
    gameModelsTold = gameModels.size();
    makeOwnCorners();
    std::unordered_map<uint32_t, bool> textures;
    size_t moving = 0;  // 3.0.20-beta.16: their corners are their own space - not checked against our models
    for (const auto& [id, n] : gameModels) {
        for (const uint32_t t : n.textures) textures[t] = true;
        if (n.moving) ++moving;
    }
    LOG_INFO("game models: %s - %zu nodes captured as the game draws them (%zu of them moving), %zu faces, %zu "
             "textures; the still ones' corners on our own models (placed from the map's files) %zu of %zu (%.1f %%)%s",
             mapName.c_str(), gameModels.size(), moving, gameModelFaces, textures.size(), gameModelCornersOurs,
             gameModelCorners, gameModelCorners ? gameModelCornersOurs * 100.0 / double(gameModelCorners) : 0.0,
             ownCornersMade ? "" : " - our own models not up yet");
}

}  // namespace uagfx::gpu
