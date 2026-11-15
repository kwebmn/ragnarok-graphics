#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "core/game_models.hpp"
#include "core/model_census.hpp"

// 3.0.20-beta.11 (the owner's «делай все сразу»: the card's way in Direct3D 11 and 12): the game's still models in the
// card's form - five uint4 a face (model_vs: each corner's x y z in the world with its colour, the three corners'
// texture coordinates, the flags 1 two-sided / 2 its node mirrored), a node's faces by texture (the game's order kept
// within one, the faces of a texture the renderer does not know left out); the colours as the game makes them
// (modelcensus::expected) with the frame's factors; the frame's faces by texture for its draws
namespace uagfx::gamemodels {

constexpr uint32_t kCardFaceWords = 20;

// the frame's colouring: the game's way (lit or plain), its factors, alpha 0xFF; as a key (never 0)
modelcensus::Colouring colouringOf(const Frame& f);
uint64_t colouringKey(const Frame& f);

// 3.0.20-beta.12: a node's faces of one texture - the card's faces first.. first + count, their box in the world
struct CardRun {
    uint32_t texture = 0, first = 0, count = 0;
    float lo[3] = {0.0f, 0.0f, 0.0f}, hi[3] = {0.0f, 0.0f, 0.0f};
};

// the frame's faces by texture: the textures in the order first met, each one's face numbers; nodes found, ids not
struct CardFrame {
    std::vector<uint32_t> textures;
    std::vector<std::vector<uint32_t>> faces;
    uint32_t nodes = 0, unknown = 0;
};

class CardStore {
public:
    bool add(const Node& n);  // false: known already
    void clear();
    // every corner's colour with `how` (key: the frame's colouring, never 0); a node added later coloured at once
    void colour(const modelcensus::Colouring& how, uint64_t key);
    uint64_t colouredWith() const { return key_; }
    const std::vector<uint32_t>& words() const { return words_; }
    uint32_t faces() const { return uint32_t(words_.size() / kCardFaceWords); }
    size_t nodes() const { return nodes_.size(); }
    void frameOf(const std::vector<uint32_t>& ids, CardFrame& out) const;
    // 3.0.20-beta.12 (stage 3: our models in the torches' shadows and the water's reflections): the frame's nodes'
    // runs, and a run's corners appended as the casters' vertices - x y z, u v, the colour's bits as a float
    void runsOf(const std::vector<uint32_t>& ids, std::vector<CardRun>& out) const;
    void cornersOf(const CardRun& r, std::vector<float>& out) const;

private:
    struct Kept {
        Node node;
        uint32_t first = 0;           // its first face
        std::vector<uint16_t> order;  // the node's faces in the card's order
        std::vector<CardRun> runs;
    };
    void colourOf(Kept& k);
    std::unordered_map<uint32_t, Kept> nodes_;
    std::vector<uint32_t> words_;
    modelcensus::Colouring how_;
    uint64_t key_ = 0;
    mutable std::unordered_map<uint32_t, uint32_t> at_;  // frameOf's: a texture's place in the frame
};

}  // namespace uagfx::gamemodels
