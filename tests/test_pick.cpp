#include <cstring>
#include <vector>

#include "core/d3d7.hpp"
#include "core/frame.hpp"
#include "core/pick.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A TL1 vertex (32 bytes): x, y, z, rhw, then colours and uv
void vertex(std::vector<uint8_t>& out, float x, float y) {
    uint8_t v[32] = {};
    const float xyzw[4] = {x, y, 0.5f, 1.0f};
    std::memcpy(v, xyzw, sizeof xyzw);
    out.insert(out.end(), v, v + 32);
}

// A quad from (x0, y0) to (x1, y1) as a triangle strip of texture `tex`
void quad(FrameRecord& f, float x0, float y0, float x1, float y1, uint32_t tex,
          uint32_t primitive = d3d7::PT_TRIANGLESTRIP) {
    std::vector<uint8_t> v;
    vertex(v, x0, y0);
    vertex(v, x1, y0);
    vertex(v, x0, y1);
    vertex(v, x1, y1);
    DrawCmd d;
    d.state = f.addState(DrawSetup{});
    d.primitive = primitive;
    d.fvf = d3d7::FVF_TL1;
    d.texture[0] = tex;
    if (primitive == d3d7::PT_TRIANGLELIST) {
        const uint16_t idx[6] = {0, 1, 2, 2, 1, 3};
        f.draw(d, v.data(), 4, idx, 6);
    } else {
        f.draw(d, v.data(), 4, nullptr, 0);
    }
}

}  // namespace

TEST(pick_finds_the_draws_under_the_point) {
    // 2.3, uaRO Studio (L2): the textures under the cursor - the frame's draws whose triangles hold its pixel, the
    // one on top first
    FrameRecord f;
    quad(f, 0, 0, 100, 100, 7);                         // the ground
    quad(f, 40, 40, 60, 60, 9, d3d7::PT_TRIANGLELIST);  // a sprite over it, indexed
    quad(f, 200, 200, 210, 210, 11);                    // something elsewhere
    const auto any = [](const DrawCmd& d) { return d.texture[0] != 0; };
    CHECK(pick::drawsAt(f, 50, 50, any) == (std::vector<int>{1, 0}));
    CHECK(pick::drawsAt(f, 10, 90, any) == std::vector<int>{0});
    CHECK(pick::drawsAt(f, 150, 150, any).empty());
    // what the caller does not want (the interface over the world, say) is passed over
    const auto notNine = [](const DrawCmd& d) { return d.texture[0] != 9; };
    CHECK(pick::drawsAt(f, 50, 50, notNine) == std::vector<int>{0});
}

TEST(pick_edges_count_once) {
    CHECK(pick::inTriangle(0, 0, 10, 0, 0, 10, 2, 2));
    CHECK(!pick::inTriangle(0, 0, 10, 0, 0, 10, 8, 8));
    CHECK(pick::inTriangle(0, 0, 0, 10, 10, 0, 2, 2));  // either winding
    CHECK(pick::inTriangle(0, 0, 10, 0, 0, 10, 0, 5));  // on an edge: in
    CHECK(!pick::inTriangle(0, 0, 10, 0, 20, 0, 5, 0));  // a triangle of no area holds nothing
    // a fan of the corners 0,1,2,3 in a strip's order covers (0,1,2) and (0,2,3): not the right one's middle
    FrameRecord f;
    quad(f, 0, 0, 10, 10, 3, d3d7::PT_TRIANGLEFAN);
    const auto all = [](const DrawCmd&) { return true; };
    CHECK(pick::drawsAt(f, 9, 5, all).empty());
    CHECK(pick::drawsAt(f, 1, 9, all) == std::vector<int>{0});
}

TEST(pick_request_waits_for_the_next_frame) {
    // 2.3.1: the key is read at a present, after that frame's draws were played - the pick looks through the next
    // frame's (2.3.0 wrote "nothing under the cursor" at once, every time)
    pick::Request r;
    uint32_t hit = 99;
    CHECK(!r.looking() && !r.due(hit));
    r.ask(10.5f, 20.5f, 1000);
    CHECK(r.looking() && r.x() == 10.5f && r.y() == 20.5f);
    CHECK(!r.due(hit));  // the frame it was asked at: its draws are gone
    r.seen({}, {});      // the next frame's first record: nothing there
    r.seen({7}, {});     // its second: a texture on top
    CHECK(r.due(hit) && hit == 7);
    CHECK(!r.looking() && !r.due(hit));  // once
    r.ask(300, 300, 2000);
    r.seen({}, {});
    CHECK(r.due(hit) && hit == 0);  // nothing of the AI's there: said so
}

TEST(pick_request_takes_the_opaque_first_and_goes_down) {
    // 2.3.1: the game draws the cursor's cell (grid.tga) blended over the ground right under the cursor - the first
    // press takes what is drawn opaque (the ground, a model), each press again at the same place the next one down
    pick::Request r;
    uint32_t hit = 0;
    const auto press = [&](float x, uint64_t ms, std::vector<uint32_t> opaque, std::vector<uint32_t> blended) {
        r.ask(x, 50.0f, ms);
        r.seen(opaque, blended);
        return r.due(hit) ? hit : 999u;
    };
    CHECK_EQ(press(50, 1000, {5}, {3}), 5u);    // the ground, not the cell
    CHECK_EQ(press(51, 2000, {5}, {3}), 3u);    // again, a pixel off: the cell under the cursor
    CHECK_EQ(press(50, 3000, {5}, {3}), 5u);    // and round again
    CHECK_EQ(press(80, 4000, {5}, {3}), 5u);    // another place: from the top
    CHECK_EQ(press(80, 30000, {5}, {3}), 5u);   // the same place much later: from the top
    // two records: the later one's draws lie over the earlier one's; a texture seen twice counts once
    r.ask(10, 10, 40000);
    r.seen({4}, {});
    r.seen({9, 4}, {3});
    CHECK(r.due(hit) && hit == 9);
    r.ask(10, 10, 41000);
    r.seen({4}, {});
    r.seen({9, 4}, {3});
    CHECK(r.due(hit) && hit == 4);
    r.ask(10, 10, 42000);
    r.seen({4}, {});
    r.seen({9, 4}, {3});
    CHECK(r.due(hit) && hit == 3);
}
