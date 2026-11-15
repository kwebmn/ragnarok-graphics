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
