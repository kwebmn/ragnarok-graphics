#include <algorithm>
#include <cstring>

#include "core/game_models_place.hpp"
#include "core/model_census.hpp"
#include "win/gpu/impl.hpp"
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
    for (size_t i = 0; i + 2 < n.vertices.size(); i += 3) {
        ++gameModelCorners;
        if (ownCorners.within(&n.vertices[i], 0.5f)) ++gameModelCornersOurs;
    }
}

// 3.0.20-beta.4: the frame's game models, hashed (FNV-1a) - the own ground's record is made again when they change
uint64_t Renderer::Impl::gameFrameKey() const {
    if (gameModelsNow.frame().ids.empty()) return 0;
    uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
    };
    mix(gameModelsNow.frame().ids.data(), gameModelsNow.frame().ids.size() * sizeof(uint32_t));
    mix(gameModelsNow.frame().factors, sizeof gameModelsNow.frame().factors);
    const uint8_t lit = gameModelsNow.frame().lit ? 1 : 0;
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
    const std::vector<uint32_t>& ids = gameModelsNow.frame().ids;
    if (ids.empty() || !o.modelLikeKnown) return;
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
    const size_t jobs = (ids.size() + kGameNodesAJob - 1) / kGameNodesAJob;
    if (gameOuts.size() < jobs) gameOuts.resize(jobs);
    if (gameScratch.size() < jobs) gameScratch.resize(jobs);
    parallelChunks(jobs, [&](size_t job) {
        gamemodels::Placed& out = gameOuts[job];
        out.clear();
        const size_t end = std::min(ids.size(), (job + 1) * kGameNodesAJob);
        for (size_t i = job * kGameNodesAJob; i < end; ++i) {
            const auto it = gameModels.find(ids[i]);
            if (it == gameModels.end())
                ++out.unknown;  // 3.0.20-beta.7
            else
                gamemodels::place(it->second, pic, gameScratch[job], out);
        }
    });
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

// 3.0.20-beta.9: as the own ground's draws (drawOwnGround) - after the game's draws gathered so far, the own pass
// (no sway of leaves - a draw holds many trees -, not a caster)
bool Renderer::Impl::drawGameModelsAlone() {
    gameAloneDrawn = true;
    OwnState& o = ownState;
    own_ground::View view;
    const bool ready = o.modelLikeKnown && viewToWorldValid && own_ground::viewOf(viewToWorld, view);
    if (ready && !gameModelsNow.frame().ids.empty() && cardModelsReady()) {  // 3.0.20-beta.11: from the card
        gameFog.fit();
        drawGameModelsCard(gameFog);
    } else if (ready && !gameModelsNow.frame().ids.empty()) {
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
    }
    if (ready) drawGameLeaves(gameFog);  // 3.0.20-beta.13
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
    desc.Width = UINT64(room) * gamemodels::kCardFaceWords * 4;
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
    v.Buffer.NumElements = room * (gamemodels::kCardFaceWords / 4);
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
    if (frame.ids.empty() || !o.modelLikeKnown || !viewToWorldValid) return;
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
    gameCards.frameOf(frame.ids, gameCardFrame);
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
        cardPacked.resize((list.size() + 7) / 8 * 8, 0u);
        DrawCmd cmd;
        cmd.state = state;
        cmd.texture[0] = gameCardFrame.textures[t];
        cmd.primitive = d3d7::PT_TRIANGLELIST;
        cmd.fvf = d3d7::FVF_TL1;
        cardRec.draw(cmd, cardPacked.data(), uint32_t(cardPacked.size() / 8), nullptr, 0);
        cardCounts.push_back(UINT(list.size()));
        o.gameTriangles += list.size();
    }
    o.gameNodes = gameCardFrame.nodes;
    o.gameUnknown = gameCardFrame.unknown;
    if (cardCounts.empty()) return;
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
    geometryList = 0;  // the game's record's vertices go up again with its next draw
    noteGameCasters();  // 3.0.20-beta.12: the reflections and the torches' shadows take them
    ++gameDrawnFrames;
    ++gameDrawnByCard;
    gameDrawnTriangles += o.gameTriangles;
    gameDrawnNodes += o.gameNodes;
    gameDrawnUnknown += o.gameUnknown;
}

// 3.0.20-beta.13 (the owner: «не качаются деревья»): the frame's game models' leaves drawn as the game's own trees - a
// draw a node's leafy texture (as its own draw items), placed by the processor, coloured as the game, through the
// world's path: the wind's sway by the draw's extent, the crowns' volume, the light through, the tints by its place,
// its casters noted as the game's
void Renderer::Impl::drawGameLeaves(const own_ground::Shading& fog) {
    const gamemodels::Frame& frame = gameModelsNow.frame();
    if (!leavesOwnWay() || frame.ids.empty() || !ownState.modelLikeKnown || !viewToWorldValid) return;
    gamemodels::Picture pic;
    if (!own_ground::viewOf(viewToWorld, pic.view)) return;
    pic.p = camera::projectionFor(gameWidth, gameHeight);
    pic.how = gamemodels::colouringOf(frame);
    pic.fog = fog.fogged() ? &fog : nullptr;
    pic.plain = fog.specularAt(0.0f);
    leafRec.reset();
    DrawSetup st = ownState.modelLike;
    st.cull = d3d7::CULL_NONE;  // culled by place() as the game culls
    const uint32_t state = leafRec.addState(st);
    for (const uint32_t id : frame.ids) {
        const auto it = gameModels.find(id);
        if (it == gameModels.end()) continue;
        const gamemodels::Node& n = it->second;
        leafWanted.assign(n.textures.size(), false);
        bool any = false;
        for (size_t t = 0; t < n.textures.size(); ++t)
            if (n.textures[t] && leafyTexture(n.textures[t])) leafWanted[t] = any = true;
        if (!any) continue;
        leafOut.clear();
        gamemodels::place(n, pic, leafScratch, leafOut, &leafWanted);
        for (size_t t = 0; t < leafOut.textures(); ++t) {
            const std::vector<own_ground::Tl1>& v = leafOut.vertices(t);
            if (v.empty() || v.size() > 65535) continue;
            DrawCmd cmd;
            cmd.state = state;
            cmd.texture[0] = leafOut.texture(t);
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

void Renderer::Impl::tellGameModels() {
    if (gameDrawnFrames) {  // 3.0.20-beta.7
        LOG_INFO("game models: drawn by us in %llu pictures (%llu of them from the card) - %.0f triangles a picture, "
                 "%.0f nodes, %.1f of the frame's nodes unknown to the renderer",
                 static_cast<unsigned long long>(gameDrawnFrames), static_cast<unsigned long long>(gameDrawnByCard),
                 double(gameDrawnTriangles) / double(gameDrawnFrames), double(gameDrawnNodes) / double(gameDrawnFrames),
                 double(gameDrawnUnknown) / double(gameDrawnFrames));
        gameDrawnFrames = gameDrawnTriangles = gameDrawnNodes = gameDrawnUnknown = gameDrawnByCard = 0;
    }
    if (gameModels.size() == gameModelsTold) return;
    gameModelsTold = gameModels.size();
    makeOwnCorners();
    std::unordered_map<uint32_t, bool> textures;
    for (const auto& [id, n] : gameModels)
        for (const uint32_t t : n.textures) textures[t] = true;
    LOG_INFO("game models: %s - %zu static nodes captured as the game draws them, %zu faces, %zu textures; their "
             "corners on our own models (placed from the map's files) %zu of %zu (%.1f %%)%s",
             mapName.c_str(), gameModels.size(), gameModelFaces, textures.size(), gameModelCornersOurs,
             gameModelCorners, gameModelCorners ? gameModelCornersOurs * 100.0 / double(gameModelCorners) : 0.0,
             ownCornersMade ? "" : " - our own models not up yet");
}

}  // namespace uagfx::gpu
