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

// 3.0.20-beta.12 (the owner's «да, делай» - stage 3: our models in the torches' shadows and the water's reflections):
// the frame's nodes' runs - a node's faces of one texture - with their boxes in the world, and a run's corners as the
// casters' vertices (x y z, u v, the colour's bits as a float), as the renderers collect the game's own opaque draws
TEST(game_models_card_runs_and_their_corners_as_casters) {
    gamemodels::CardStore store;
    store.add(nodeOf(5));
    modelcensus::Colouring plain{modelcensus::Way::Plain, {1.0f, 1.0f, 1.0f}, 0xFF};
    store.colour(plain, 11);
    std::vector<gamemodels::CardRun> runs;
    store.runsOf({5, 77}, runs);  // 77 unknown
    CHECK(runs.size() == 2 && runs[0].texture == 7 && runs[0].count == 1 && runs[1].texture == 9 &&
          runs[1].count == 2);
    CHECK(runs[0].lo[0] == 1.0f && runs[0].lo[1] == 2.0f && runs[0].lo[2] == 3.0f);
    CHECK(runs[0].hi[0] == 7.0f && runs[0].hi[1] == 8.0f && runs[0].hi[2] == 9.0f);
    std::vector<float> corners;
    store.cornersOf(runs[1], corners);  // texture 9: faces 0 and 2
    CHECK(corners.size() == 2 * 3 * 6);
    CHECK(corners[0] == 1.0f && corners[1] == 2.0f && corners[2] == 3.0f);  // face 0, corner 0
    CHECK(corners[3] == 0.0f && corners[4] == 0.1f);                        // its uv
    uint32_t colour;
    std::memcpy(&colour, &corners[5], sizeof colour);
    CHECK(colour == (0xFF000000u | 0x00102030u));
    CHECK(corners[6] == 4.0f && corners[18 + 3] == 2.0f);  // corner 1's x; face 2's first u
}

// 3.0.20-beta.14 (stage 4): a moving node's faces kept in its own space - left out of the frame's batches (a draw a
// run of its, with its matrix), its runs found by its id, its corners into the world by a matrix for the casters
TEST(game_models_card_moving_nodes_drawn_apart) {
    gamemodels::CardStore store;
    gamemodels::Node n = nodeOf(5);
    n.moving = true;
    store.add(n);
    store.add(nodeOf(6));
    gamemodels::CardFrame frame;
    store.frameOf({5, 6}, frame);  // the moving one's faces not in the batches
    CHECK(frame.nodes == 1 && frame.textures.size() == 2 && frame.faces[0].size() == 1 && frame.faces[1].size() == 2);
    const std::vector<gamemodels::CardRun>* runs = store.runsOfNode(5);
    CHECK(runs && runs->size() == 2 && store.moving(5) && !store.moving(6) && !store.runsOfNode(77));
    std::vector<float> corners;
    const float moved[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 100, 0, 0};
    store.cornersOf((*runs)[0], corners, moved);
    CHECK(corners.size() == 18 && corners[0] == 101.0f && corners[1] == 2.0f && corners[6] == 104.0f);
}

// 3.0.20-beta.14: the frame's moving nodes' faces join the batches by texture - each face's number with its node's
// place in the frame's moving list (from 1) in the high bits, model_vs puts those faces by that node's matrix; an
// unknown one keeps its place (the frame's matrices go to the card in the list's order)
TEST(game_models_card_moving_faces_join_the_batches_with_their_place) {
    gamemodels::CardStore store;
    gamemodels::Node n = nodeOf(5);
    n.moving = true;
    store.add(n);          // the card's faces 0 (texture 7), 1 and 2 (texture 9)
    store.add(nodeOf(6));  // 3 (texture 7), 4 and 5 (texture 9)
    gamemodels::CardFrame frame;
    store.frameOf({6}, {77, 5}, frame);  // 77 unknown: place 1 left unused
    CHECK(frame.nodes == 2 && frame.moving == 1 && frame.unknown == 1);
    CHECK(frame.textures.size() == 2 && frame.textures[0] == 7 && frame.textures[1] == 9);
    const uint32_t second = 2u << gamemodels::kCardSlotShift;
    CHECK(frame.faces[0] == (std::vector<uint32_t>{3, 0 | second}));
    CHECK(frame.faces[1] == (std::vector<uint32_t>{4, 5, 1 | second, 2 | second}));
    CHECK(gamemodels::kCardMostMoving == (1u << (32 - gamemodels::kCardSlotShift)) - 1);
    CHECK(gamemodels::kCardMostFaces == 1u << gamemodels::kCardSlotShift);
}

// 3.0.20-beta.37 (the owner's «6,7,8,9», 9 - the frame's work on more threads): the frame's faces gathered in parts -
// each a run of its nodes (the still ones, then the moving), on any thread - and put together in their order are the
// frame's faces gathered at once: the same textures in the same order, the same faces, the same counts
TEST(game_models_card_frame_in_parts_is_the_frame_at_once) {
    gamemodels::CardStore store;
    std::vector<uint32_t> ids, moving;
    for (uint32_t id = 1; id <= 40; ++id) {
        gamemodels::Node n = nodeOf(id);
        n.textures = {9 + id % 3, 7 + id % 5, 0};  // textures first met all along
        n.moving = id % 9 == 0;
        store.add(n);
        (n.moving ? moving : ids).push_back(id);
    }
    ids.push_back(77);  // unknown
    gamemodels::CardFrame once;
    store.frameOf(ids, moving, once);
    for (size_t parts : {size_t(1), size_t(3), size_t(7), size_t(64)}) {
        std::vector<gamemodels::FramePart> got(parts);
        for (size_t k = 0; k < parts; ++k) store.framePart(ids, moving, k, parts, got[k]);
        gamemodels::CardFrame joined;
        gamemodels::joinParts(got, parts, joined);
        CHECK(joined.textures == once.textures);
        CHECK(joined.nodes == once.nodes && joined.unknown == once.unknown && joined.moving == once.moving);
        for (size_t t = 0; t < once.textures.size(); ++t) CHECK(joined.faces[t] == once.faces[t]);
    }
}
