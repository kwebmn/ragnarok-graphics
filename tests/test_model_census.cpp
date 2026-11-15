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
    const modelcensus::Check c = modelcensus::compare(faces, drawn.data(), drawn.size());
    CHECK(c.faces == 3 && c.vertices == 9 && c.same == 9 && c.mostApart == 0);
}

TEST(model_census_colours_apart_counted_with_their_ratio) {
    std::vector<modelcensus::FaceColours> faces = {face(0x406080, 0.1f), face(0x2040A0, 0.2f)};
    std::vector<modelcensus::DrawnVertex> drawn;
    drawnOf(faces[0], drawn, 0x80000000u, 0.5);  // half as bright (and half see-through: the alpha left out)
    drawnOf(faces[1], drawn, 0x80000000u, 0.5);
    const modelcensus::Check c = modelcensus::compare(faces, drawn.data(), drawn.size());
    CHECK(c.faces == 2 && c.vertices == 6 && c.same == 0);
    CHECK(c.ratios > 0 && c.ratio / c.ratios > 0.47 && c.ratio / c.ratios < 0.53);
    CHECK(c.mostApart >= 0x50 && c.mostApart <= 0x51);  // 0xA2 -> 0x51
}

TEST(model_census_tally_tells_the_models_share_and_the_check) {
    modelcensus::Tally t;
    CHECK(t.line().empty());
    for (int i = 0; i < 10; ++i) t.frame(modelcensus::Frame{1400, 26000, 2.5}, 10.0);
    modelcensus::Check same;
    same.faces = 100;
    same.vertices = 300;
    same.same = 297;
    same.ratio = 3.0 * 0.5;
    same.ratios = 3;
    same.mostApart = 12;
    t.check(same, modelcensus::Way::Plain);
    t.check(same, modelcensus::Way::Night);
    const std::string line = t.line();
    CHECK(has(line, "models: the game's own a frame - 1400 nodes, 26000 faces, 2.50 ms (25 % of its frame)"));
    CHECK(has(line, "colours: 600 vertices of 200 faces checked - 594 (99.0 %) the very same as in its memory"));
    CHECK(has(line, "the rest x0.50 on average (12 apart at most)"));
    CHECK(has(line, "ways: plain 1, night 1, half see-through 0, other 0"));
    t.clear();
    CHECK(t.line().empty());
}
