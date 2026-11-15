// 3.0.20-beta.11 (the owner's «делай все сразу»: the card's way in both renderers): the game's still models kept in the
// card's form - five uint4 a face (each corner's x y z and colour, the texture coordinates, the flags) by texture, the
// colours as the game makes them, the frame's faces by texture for its draws
#include <cstring>
#include <vector>

#include "core/game_models_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

uint32_t bitsOf(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

gamemodels::Node nodeOf(uint32_t id) {
    gamemodels::Node n;
    n.id = id;
    n.map = 1;
    n.textures = {9, 7, 0};  // the third unknown to the renderer
    n.vertices = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    for (int t = 0; t < 4; ++t) {
        gamemodels::Face f;
        f.v[0] = 0, f.v[1] = 1, f.v[2] = 2;
        for (int i = 0; i < 6; ++i) f.uv[i] = float(t) + 0.1f * float(i);
        for (int i = 0; i < 3; ++i) f.colour[i] = 0x00102030u * uint32_t(t + 1) + uint32_t(i);
        f.texture = uint8_t(t == 3 ? 2 : t % 2);  // faces 0, 2: texture 9; face 1: texture 7; face 3: unknown
        f.flags = t == 1 ? gamemodels::kTwoSided : 0;
        n.faces.push_back(f);
    }
    n.flipped = true;
    return n;
}

}  // namespace

TEST(game_models_card_keeps_a_nodes_faces_by_texture) {
    gamemodels::CardStore store;
    CHECK(store.add(nodeOf(5)));
    CHECK(!store.add(nodeOf(5)));  // known: kept once
    CHECK(store.faces() == 3);  // the unknown texture's face left out
    const std::vector<uint32_t>& w = store.words();
    CHECK(w.size() == 3 * gamemodels::kCardFaceWords);
    // by texture, the game's order within one: texture 7 (face 1), then 9 (faces 0, 2)
    CHECK(w[12] == bitsOf(1.0f) && w[18] == (1u | 2u));         // face 1: its uv, two-sided and mirrored
    CHECK(w[20 + 12] == bitsOf(0.0f) && w[20 + 18] == 2u);       // face 0
    CHECK(w[40 + 12] == bitsOf(2.0f));                           // face 2
    CHECK(w[0] == bitsOf(1.0f) && w[1] == bitsOf(2.0f) && w[6] == bitsOf(6.0f) && w[8] == bitsOf(7.0f));
    gamemodels::CardFrame frame;
    store.frameOf({5, 77}, frame);  // 77 unknown
    CHECK(frame.nodes == 1 && frame.unknown == 1);
    CHECK(frame.textures.size() == 2 && frame.textures[0] == 7 && frame.textures[1] == 9);
    CHECK(frame.faces[0] == (std::vector<uint32_t>{0}) && frame.faces[1] == (std::vector<uint32_t>{1, 2}));
}

TEST(game_models_card_colours_as_the_game_and_again_when_they_change) {
    gamemodels::CardStore store;
    store.add(nodeOf(5));
    const std::vector<uint32_t>& w = store.words();
    CHECK(w[3] == 0 && store.colouredWith() == 0);  // none yet
    modelcensus::Colouring plain{modelcensus::Way::Plain, {1.0f, 1.0f, 1.0f}, 0xFF};
    store.colour(plain, 11);
    CHECK(w[3] == (0xFF000000u | 0x00204060u) && w[7] == (0xFF000000u | 0x00204061u));  // face 1's corners
    CHECK(store.colouredWith() == 11);
    store.add(nodeOf(6));  // a node after: coloured at once
    CHECK(store.words()[60 + 3] == (0xFF000000u | 0x00204060u));
    modelcensus::Colouring half{modelcensus::Way::Plain, {0.5f, 0.5f, 0.5f}, 0xFF};
    store.colour(half, 12);
    CHECK(store.words()[3] == (0xFF000000u | 0x00102030u));
    store.clear();
    CHECK(store.faces() == 0 && store.words().empty());
}

TEST(game_models_card_colouring_of_a_frame_and_its_key) {
    gamemodels::Frame f;
    f.factors[0] = 0.5f;
    f.lit = true;
    const modelcensus::Colouring how = gamemodels::colouringOf(f);
    CHECK(how.way == modelcensus::Way::Lit && how.factors[0] == 0.5f && how.factors[1] == 1.0f && how.alpha == 0xFF);
    const uint64_t key = gamemodels::colouringKey(f);
    CHECK(key != 0 && key == gamemodels::colouringKey(f));
    f.factors[2] = 0.75f;
    CHECK(gamemodels::colouringKey(f) != key);
    f.factors[2] = 1.0f;
    f.lit = false;
    CHECK(gamemodels::colouringKey(f) != key);
}
