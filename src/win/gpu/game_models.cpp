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
        gameModels.clear();
        gameModelFaces = gameModelCorners = gameModelCornersOurs = gameModelsTold = 0;
        ownCorners.clear();
        ownCornersMade = false;
        gameModelsUnchecked.clear();
        gameModelsMap = n.map;
    }
    const auto [it, fresh] = gameModels.insert_or_assign(n.id, n);
    if (!fresh) return;  // the same node again: the first one kept its count
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
    if (ready && !gameModelsNow.frame().ids.empty()) {
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

void Renderer::Impl::tellGameModels() {
    if (gameDrawnFrames) {  // 3.0.20-beta.7
        LOG_INFO("game models: drawn by us in %llu pictures - %.0f triangles a picture, %.0f nodes, %.1f of the "
                 "frame's nodes unknown to the renderer",
                 static_cast<unsigned long long>(gameDrawnFrames), double(gameDrawnTriangles) / double(gameDrawnFrames),
                 double(gameDrawnNodes) / double(gameDrawnFrames), double(gameDrawnUnknown) / double(gameDrawnFrames));
        gameDrawnFrames = gameDrawnTriangles = gameDrawnNodes = gameDrawnUnknown = 0;
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
