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
    if (gameFrame.ids.empty()) return 0;
    uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
    };
    mix(gameFrame.ids.data(), gameFrame.ids.size() * sizeof(uint32_t));
    mix(gameFrame.factors, sizeof gameFrame.factors);
    const uint8_t lit = gameFrame.lit ? 1 : 0;
    mix(&lit, 1);
    return h | 1;
}

// 3.0.20-beta.4 (stage 2b of «Модели нашим рендером»): the game's still models the driver left out of its draw this
// frame (gameFrame.ids), from their captured nodes - each corner placed as the own ground's, coloured as the game
// colours it (core/model_census expected: its lit ints or colour bytes x the factors), its texture the game's own; with
// the game's model state (both sides drawn, as the models past the far plane), into the own ground's record (layer 3)
void Renderer::Impl::drawGameModels(const own_ground::View& view, const camera::Projection& p) {
    OwnState& o = ownState;
    o.gameTriangles = 0;
    if (gameFrame.ids.empty() || !o.modelLikeKnown) return;
    DrawSetup ms = o.modelLike;
    ms.cull = d3d7::CULL_NONE;
    const uint32_t state = o.rec.addState(ms);
    const bool fogged = o.shading.fogged();
    const uint32_t plain = o.shading.specularAt(0.0f);
    modelcensus::Colouring how;
    how.way = gameFrame.lit ? modelcensus::Way::Lit : modelcensus::Way::Plain;
    std::copy(gameFrame.factors, gameFrame.factors + 3, how.factors);
    how.alpha = 0xFF;
    for (auto& [id, pd] : gamePending) pd.verts.clear(), pd.idx.clear();
    const auto emit = [&](uint32_t texture) {
        OwnState::ModelPending& pd = gamePending[texture];
        if (!pd.idx.empty()) {
            DrawCmd cmd;
            cmd.state = state;
            cmd.texture[0] = texture;
            cmd.primitive = d3d7::PT_TRIANGLELIST;
            cmd.fvf = d3d7::FVF_TL1;
            o.rec.draw(cmd, pd.verts.data(), uint32_t(pd.verts.size()), pd.idx.data(), uint32_t(pd.idx.size()));
            o.drawLayer.push_back(3);
        }
        pd.verts.clear();
        pd.idx.clear();
    };
    for (const uint32_t id : gameFrame.ids) {
        const auto it = gameModels.find(id);
        if (it == gameModels.end()) continue;
        const gamemodels::Node& n = it->second;
        for (const gamemodels::Face& f : n.faces) {
            const uint32_t texture = f.texture < n.textures.size() ? n.textures[f.texture] : 0;
            if (!texture) continue;
            own_ground::Tl1 t[3];
            bool ok = true;
            for (int k = 0; k < 3 && ok; ++k) {
                float seen[3];
                own_ground::toView(view, &n.vertices[size_t(f.v[k]) * 3], seen);
                own_ground::Tl2 placed;
                ok = own_ground::placeView(p, seen, placed);
                t[k].x = placed.x, t[k].y = placed.y, t[k].z = placed.z, t[k].rhw = placed.rhw;
                t[k].diffuse = modelcensus::expected(how, f.colour[k], &f.lit[3 * k]);
                t[k].specular = fogged ? o.shading.specularAt(seen[2]) : plain;
                t[k].u = f.uv[2 * k], t[k].v = f.uv[2 * k + 1];
            }
            if (!ok) continue;  // a corner behind the eye: as the game, not drawn
            // 3.0.20-beta.4: culled as the game's draw culls - by its turn on the screen, two-sided ones never
            const float s0[2] = {t[0].x, t[0].y}, s1[2] = {t[1].x, t[1].y}, s2[2] = {t[2].x, t[2].y};
            if (!gamemodels::shown(s0, s1, s2, (f.flags & gamemodels::kTwoSided) != 0, n.flipped)) continue;
            OwnState::ModelPending& pd = gamePending[texture];
            if (pd.verts.size() + 3 > 65535) emit(texture);
            const uint16_t base = uint16_t(pd.verts.size());
            for (int k = 0; k < 3; ++k) {
                pd.verts.push_back(t[k]);
                pd.idx.push_back(uint16_t(base + k));
            }
            ++o.gameTriangles;
        }
    }
    for (auto& [texture, pd] : gamePending) emit(texture);
}

void Renderer::Impl::tellGameModels() {
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
