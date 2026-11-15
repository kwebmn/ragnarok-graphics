// 3.0.20-beta.1 (the owner's «приступай» - stage 1 of «Модели нашим рендером»: the models' colours read from the game's
// memory and set against its own picture, the models' share of its frame): a node's drawn vertices matched to its
// faces in memory by their texture coordinates, the colours compared channel by channel
#include <string>
#include <vector>

#include "core/model_census.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

modelcensus::FaceColours face(uint32_t colour, float u) {
    modelcensus::FaceColours f;
    for (int i = 0; i < 3; ++i) f.colour[i] = colour + uint32_t(i);
    for (int i = 0; i < 6; ++i) f.uv[i] = u + float(i) * 0.01f;
    return f;
}

void drawnOf(const modelcensus::FaceColours& f, std::vector<modelcensus::DrawnVertex>& out,
             uint32_t alpha = 0xFF000000u, double scale = 1.0) {
    for (int i = 0; i < 3; ++i) {
        uint32_t c = 0;
        for (int b = 0; b < 3; ++b) c |= uint32_t(double((f.colour[i] >> (8 * b)) & 0xFF) * scale) << (8 * b);
        out.push_back({alpha | c, f.uv[2 * i], f.uv[2 * i + 1]});
    }
}

bool has(const std::string& line, const std::string& part) { return line.find(part) != std::string::npos; }

}  // namespace

TEST(model_census_drawn_faces_matched_in_order_past_the_culled_ones) {
    std::vector<modelcensus::FaceColours> faces = {face(0x102030, 0.1f), face(0x405060, 0.2f), face(0x708090, 0.3f),
                                                   face(0xA0B0C0, 0.4f)};
    std::vector<modelcensus::DrawnVertex> drawn;
    drawnOf(faces[0], drawn);
    drawnOf(faces[2], drawn);  // face 1 culled
    drawnOf(faces[3], drawn);
    drawn.push_back({0x12345678u, 9.0f, 9.0f});  // the block's rest: stale, no face of its
    drawn.push_back({0x12345678u, 9.0f, 9.0f});
    drawn.push_back({0x12345678u, 9.0f, 9.0f});
    const modelcensus::Colouring plain{modelcensus::Way::Plain, {1.0f, 1.0f, 1.0f}, 0xFF};
    const modelcensus::Check c = modelcensus::compare(faces, {}, plain, drawn.data(), drawn.size());
    CHECK(c.faces == 3 && c.vertices == 9 && c.same == 9 && c.mostApart == 0);
}

// 3.0.20-beta.2 (the owner's three reports on beta.1: every node «night», 27-61 % the same, the rest x1.8-2.3): the
// game's own colouring from its code (0x4DEDB0-0x4DEF7F) - plain: the face's colour bytes x 0x7710E4/E8/EC, truncated,
// the actor's alpha; lit (0x7710B9): the vertex's ints at +0x144 (the colour plus the place's light) x the same; half
// see-through: the colour as it is, alpha 0x80; other (0x748878): the colour x 0x7710D8/DC/E0
TEST(model_census_expected_as_the_game_colours_each_way) {
    const uint32_t mem = 0x00804020;  // bytes 2 1 0: 0x80 0x40 0x20
    const int32_t lit[3] = {300, 64, 0};
    const float half[3] = {0.5f, 0.75f, 1.0f};
    CHECK(modelcensus::expected(modelcensus::Colouring{modelcensus::Way::Plain, {0.5f, 0.75f, 1.0f}, 0xC0}, mem, lit) ==
          0xC0403020u);  // 0x80 * 0.5, 0x40 * 0.75, 0x20 * 1
    CHECK(modelcensus::expected(modelcensus::Colouring{modelcensus::Way::Lit, {half[0], half[1], half[2]}, 0xFF}, mem,
                                lit) == 0xFF963000u);  // 300 * 0.5 = 150, 64 * 0.75 = 48, 0
    CHECK(modelcensus::expected(modelcensus::Colouring{modelcensus::Way::SeeThrough, {0.1f, 0.1f, 0.1f}, 0xFF}, mem,
                                lit) == 0x80804020u);
    CHECK(modelcensus::expected(modelcensus::Colouring{modelcensus::Way::Lit, {1.0f, 1.0f, 1.0f}, 0xFF}, mem, lit) ==
          0xFF2C4000u);  // 300 stored as a byte: its low 8 bits, as the game's own store
}

TEST(model_census_lit_faces_checked_against_their_vertex_light) {
    std::vector<modelcensus::FaceColours> faces = {face(0x406080, 0.1f)};
    std::vector<modelcensus::FaceLit> lits(1);
    for (int i = 0; i < 9; ++i) lits[0].rgb[i] = 100 + i;
    const modelcensus::Colouring lit{modelcensus::Way::Lit, {1.0f, 1.0f, 1.0f}, 0xFF};
    std::vector<modelcensus::DrawnVertex> drawn;
    for (int v = 0; v < 3; ++v) {
        const uint32_t c =
            0xFF000000u | uint32_t(100 + 3 * v) << 16 | uint32_t(101 + 3 * v) << 8 | uint32_t(102 + 3 * v);
        drawn.push_back({c, faces[0].uv[2 * v], faces[0].uv[2 * v + 1]});
    }
    const modelcensus::Check c = modelcensus::compare(faces, lits, lit, drawn.data(), drawn.size());
    CHECK(c.faces == 1 && c.vertices == 3 && c.same == 3);
    const modelcensus::Colouring plain{modelcensus::Way::Plain, {1.0f, 1.0f, 1.0f}, 0xFF};
    CHECK(modelcensus::compare(faces, lits, plain, drawn.data(), drawn.size()).same == 0);  // not the face's own
}

TEST(model_census_tally_tells_the_models_share_and_the_check) {
    modelcensus::Tally t;
    CHECK(t.line().empty());
    for (int i = 0; i < 10; ++i) t.frame(modelcensus::Frame{1400, 26000, 2.5}, 10.0);
    modelcensus::Check same;
    same.faces = 100;
    same.vertices = 300;
    same.same = 297;
    same.mostApart = 12;
    t.check(same, modelcensus::Way::Plain);
    t.check(same, modelcensus::Way::Lit);
    const std::string line = t.line();
    CHECK(has(line, "models: the game's own a frame - 1400 nodes, 26000 faces, 2.50 ms (25 % of its frame)"));
    CHECK(has(line, "colours: 600 vertices of 200 faces checked - 594 (99.0 %) as the game colours them from its "
                    "memory (12 apart at most)"));
    CHECK(has(line, "ways: plain 1, lit 1, half see-through 0, other 0"));
    t.clear();
    CHECK(t.line().empty());
}
