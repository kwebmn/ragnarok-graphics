// 3.0.20-beta.8 (the owner's «давай по очереди», step 1: the renderer's CPU +2 ms a frame building the game's still
// models anew whenever the view moved - R-XTEFYA): a node's faces placed for a picture on a renderer's thread of its
// own, each vertex placed once a node - the same vertices the one-thread build made
#include <functional>
#include <vector>

#include "core/game_models_place.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

gamemodels::Picture straightOn() {
    gamemodels::Picture pic;
    for (int i = 0; i < 9; ++i) pic.view.r[i] = i % 4 == 0 ? 1.0f : 0.0f;  // the view's axes the world's
    pic.view.eye[0] = pic.view.eye[1] = pic.view.eye[2] = 0.0f;
    pic.p.f0 = pic.p.f1 = 100.0f;
    pic.p.cx = pic.p.cy = 50.0f;
    pic.p.nearZ = 1.0f;
    pic.p.farZ = 1000.0f;
    pic.how = modelcensus::Colouring{modelcensus::Way::Plain, {1.0f, 1.0f, 1.0f}, 0xFF};
    pic.plain = 0xFF000000u;
    return pic;
}

gamemodels::Face faceOf(uint16_t a, uint16_t b, uint16_t c, uint8_t texture, uint8_t flags = 0) {
    gamemodels::Face f;
    f.v[0] = a, f.v[1] = b, f.v[2] = c;
    for (int i = 0; i < 6; ++i) f.uv[i] = 0.1f * float(i + 1);
    for (int i = 0; i < 3; ++i) f.colour[i] = 0x00804020u + uint32_t(i);
    f.texture = texture;
    f.flags = flags;
    return f;
}

// vertices 0, 1, 2 ten ahead - on the screen (50, 50), (60, 50), (50, 60) -, vertex 3 behind the eye
gamemodels::Node nodeOf() {
    gamemodels::Node n;
    n.textures = {7, 9, 0};  // the third one unknown to the renderer
    n.vertices = {0, 0, 10, 1, 0, 10, 0, 1, 10, 0, 0, -5};
    n.faces = {faceOf(0, 2, 1, 0),                           // turned as the game shows it
               faceOf(0, 1, 2, 0),                           // the other way: culled
               faceOf(0, 1, 2, 0, gamemodels::kTwoSided),    // two-sided: shown
               faceOf(0, 1, 3, 0, gamemodels::kTwoSided),    // a corner behind the eye: left out
               faceOf(0, 2, 1, 1),                           // the second texture
               faceOf(0, 2, 1, 2)};                          // an unknown texture: left out
    return n;
}

}  // namespace

TEST(game_models_place_a_node_as_the_game_draws_it) {
    const gamemodels::Picture pic = straightOn();
    gamemodels::Placed out;
    std::vector<gamemodels::Corner> scratch;
    gamemodels::place(nodeOf(), pic, scratch, out);
    CHECK(out.triangles == 3 && out.nodes == 1 && out.textures() == 2);
    CHECK(out.texture(0) == 7 && out.vertices(0).size() == 6);
    CHECK(out.texture(1) == 9 && out.vertices(1).size() == 3);
    const own_ground::Tl1& first = out.vertices(0)[0];  // face 0's corner 0: vertex 0
    own_ground::Tl2 want;
    const float seen[3] = {0.0f, 0.0f, 10.0f};
    CHECK(own_ground::placeView(pic.p, seen, want));
    CHECK(first.x == want.x && first.y == want.y && first.z == want.z && first.rhw == want.rhw);
    CHECK(first.diffuse == 0xFF804020u && first.specular == 0xFF000000u);
    CHECK(first.u == 0.1f && first.v == 0.2f);
    const own_ground::Tl1& second = out.vertices(0)[1];  // face 0's corner 1: vertex 2, its own colour and uv
    CHECK(second.x == 50.0f && second.y == 60.0f && second.diffuse == 0xFF804021u && second.u == 0.3f);
}

TEST(game_models_place_a_flipped_node_culls_the_other_way) {
    const gamemodels::Picture pic = straightOn();
    gamemodels::Node n = nodeOf();
    n.flipped = true;
    gamemodels::Placed out;
    std::vector<gamemodels::Corner> scratch;
    gamemodels::place(n, pic, scratch, out);
    CHECK(out.textures() == 1 && out.texture(0) == 7 && out.vertices(0).size() == 6);  // face 1 and the two-sided
    CHECK(out.vertices(0)[1].x == 60.0f);  // face 1's corner 1: vertex 1
    out.clear();
    CHECK(out.textures() == 0 && out.triangles == 0 && out.nodes == 0);
}

// 3.0.20-beta.13 (the owner: «не качаются деревья»): a node's leaves placed alone - the faces of the textures wanted
// (by the node's texture index), drawn a tree a draw through the world's path as the game's own trees
TEST(game_models_place_only_the_textures_wanted) {
    const gamemodels::Picture pic = straightOn();
    gamemodels::Placed out;
    std::vector<gamemodels::Corner> scratch;
    const std::vector<bool> leaves = {false, true, false};  // the node's second texture (renderer's 9) alone
    gamemodels::place(nodeOf(), pic, scratch, out, &leaves);
    CHECK(out.textures() == 1 && out.texture(0) == 9 && out.vertices(0).size() == 3 && out.triangles == 1);
}

// 3.0.20-beta.14 (stage 4): a moving node's own vertices put into the view by its node-to-view matrix this frame (as
// the game's draw puts them), the view's camera aside
TEST(game_models_place_a_moving_node_by_its_matrix) {
    gamemodels::Picture pic = straightOn();
    pic.view.eye[2] = 500.0f;  // a camera that would put the node elsewhere: not taken for a moving node
    gamemodels::Node n;
    n.textures = {7};
    n.vertices = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    n.faces = {faceOf(0, 2, 1, 0)};
    n.moving = true;
    const float toView[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 10};  // moved 10 ahead
    gamemodels::Placed out;
    std::vector<gamemodels::Corner> scratch;
    gamemodels::place(n, pic, scratch, out, nullptr, toView);
    CHECK(out.triangles == 1 && out.vertices(0).size() == 3);
    CHECK(out.vertices(0)[0].x == 50.0f && out.vertices(0)[0].y == 50.0f && out.vertices(0)[1].y == 60.0f);
}

// 3.0.20-beta.16 (the owner: «делать» - R-GZDPF9: our models and their leaves ~2.3 ms of the renderer's frame, the
// leaves placed on one thread): the frame's nodes' leaves a node each - a node with a leafy texture kept with those
// textures alone (one with none left out), each placed into its own output by the parallel for given (here the jobs
// taken backwards: the outputs by node all the same), a moving one by its matrix
TEST(game_models_leaves_a_node_each_whatever_the_order) {
    const gamemodels::Picture pic = straightOn();
    const gamemodels::Node a = nodeOf();  // textures 7, 9 and an unknown one
    gamemodels::Node bare = nodeOf();
    bare.textures = {5, 6, 0};  // nothing leafy
    gamemodels::Node moving;
    moving.textures = {9};
    moving.vertices = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    moving.faces = {faceOf(0, 2, 1, 0)};
    moving.moving = true;
    const float ahead[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 10};
    const auto leafy = [](uint32_t t) { return t == 9; };
    gamemodels::Leaves leaves;
    leaves.add(a, nullptr, leafy);
    leaves.add(bare, nullptr, leafy);
    leaves.add(moving, ahead, leafy);
    CHECK(leaves.size() == 2);
    std::vector<size_t> jobs;
    leaves.place(
        pic,
        [&jobs](size_t count, const std::function<void(size_t)>& body) {
            for (size_t i = count; i-- > 0;) {
                jobs.push_back(i);
                body(i);
            }
        },
        1);
    CHECK(jobs == (std::vector<size_t>{1, 0}));
    const gamemodels::Placed& first = leaves.placed(0);  // texture 9's face alone, as place() with `only`
    CHECK(first.textures() == 1 && first.texture(0) == 9 && first.vertices(0).size() == 3 && first.triangles == 1);
    const gamemodels::Placed& second = leaves.placed(1);
    CHECK(second.textures() == 1 && second.texture(0) == 9 && second.vertices(0).size() == 3);
    CHECK(second.vertices(0)[0].x == 50.0f && second.vertices(0)[0].y == 50.0f && second.vertices(0)[1].y == 60.0f);
    leaves.clear();
    CHECK(leaves.size() == 0);
}
