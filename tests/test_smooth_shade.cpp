#include <cstring>

#include "core/d3d7.hpp"
#include "core/game_camera.hpp"
#include "core/smooth_shade.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A corner at a view place (x, y, z) by a projection of f 100 about (0, 0).
smooth_shade::Corner at(float x, float y, float z, uint32_t colour) {
    smooth_shade::Corner c;
    c.x = x / z * 100.0f;
    c.y = y / z * 100.0f;
    c.rhw = 1.0f / z;
    c.colour = colour;
    return c;
}

uint32_t grey(int v) { return 0xFF000000u | uint32_t(v) << 16 | uint32_t(v) << 8 | uint32_t(v); }

}  // namespace

TEST(smooth_shade_rounds_a_shallow_fold_keeps_a_sharp_one) {
    // two faces meeting along x = 0 at a shallow angle (a rock's), each its own colour, lists of their own corners
    std::vector<smooth_shade::Corner> v = {
        at(-1, 0, 10, grey(100)), at(0, 0, 10, grey(100)), at(0, 1, 10, grey(100)),     // left, flat
        at(0, 0, 10, grey(200)), at(1, 0, 10.3f, grey(200)), at(0, 1, 10, grey(200)),  // right, turned a little
    };
    std::vector<uint32_t> tris = {0, 1, 2, 3, 4, 5};
    std::vector<smooth_shade::Change> c = smooth_shade::smooth(v, tris, 100, 100, 0, 0);
    CHECK(!c.empty());
    bool sharedMid = false;
    for (const auto& ch : c)
        if (ch.vertex == 1 || ch.vertex == 3)
            sharedMid = sharedMid || ((ch.colour & 255) > 100 && (ch.colour & 255) < 200);
    CHECK(sharedMid);
    for (const auto& ch : c) CHECK(ch.vertex != 0 && ch.vertex != 4);  // the corners no other face shares
    // folded at a right angle (a house's wall and roof): nothing changes
    v[4] = at(0, 0, 11, grey(200));
    CHECK(smooth_shade::smooth(v, tris, 100, 100, 0, 0).empty());
}

TEST(smooth_shade_leaves_a_draw_lit_smoothly) {
    std::vector<smooth_shade::Corner> v = {at(-1, 0, 10, grey(100)), at(0, 0, 10, grey(150)), at(0, 1, 10, grey(120)),
                                          at(1, 0, 10.3f, grey(200))};
    std::vector<uint32_t> tris = {0, 1, 2, 1, 3, 2};
    CHECK(smooth_shade::smooth(v, tris, 100, 100, 0, 0).empty());
}

namespace {

// A TL1 vertex of the record (FVF 0x1C4): its screen place by the game's projection, a colour.
struct Tl1 {
    float x, y, z, rhw;
    uint32_t diffuse, specular;
    float u, v;
};

Tl1 tl1(const camera::Projection& p, float x, float y, float z, uint32_t colour) {
    return Tl1{x / z * p.f0 + p.cx, y / z * p.f1 + p.cy, 0.5f, 1.0f / z, colour, 0, 0, 0};
}

}  // namespace

TEST(smooth_shade_record_reaches_face_lit_models_past_smooth_ones) {
    // 2.6.6: the triangles of every opaque model counted against the frame's 60000 - smoothly lit ones, which need
    // nothing, could use them up before the face-lit ones came. Only the face-lit ones count now.
    const camera::Projection p = camera::projectionFor(640, 480);
    FrameRecord rec;
    DrawSetup s;
    s.zEnable = true;
    s.zWrite = true;
    DrawCmd d;
    d.state = rec.addState(s);
    d.fvf = d3d7::FVF_TL1;
    d.primitive = d3d7::PT_TRIANGLELIST;
    std::vector<Tl1> smooth;  // a smoothly lit model: each corner its own light
    for (int t = 0; t < 200; ++t)
        for (int k = 0; k < 3; ++k)
            smooth.push_back(tl1(p, float(t % 20) + float(k == 1), float(t / 20) + float(k == 2), 10.0f,
                                 grey(60 + (t * 3 + k) % 150)));
    rec.draw(d, smooth.data(), uint32_t(smooth.size()), nullptr, 0);
    const std::vector<Tl1> rock = {  // a rock face-lit: two faces at a shallow fold
        tl1(p, -1, 0, 10, grey(100)), tl1(p, 0, 0, 10, grey(100)), tl1(p, 0, 1, 10, grey(100)),
        tl1(p, 0, 0, 10, grey(200)), tl1(p, 1, 0, 10.3f, grey(200)), tl1(p, 0, 1, 10, grey(200)),
    };
    rec.draw(d, rock.data(), uint32_t(rock.size()), nullptr, 0);
    size_t draws = 0;
    const std::vector<smooth_shade::RecordChange> c = smooth_shade::recordChanges(rec, 640, 480, draws, 100);
    CHECK_EQ(draws, size_t(1));
    CHECK(!c.empty());
    // and the face-lit ones are what the budget is for: past it, a face-lit model is left as it is
    FrameRecord many;
    d.state = many.addState(s);
    for (int m = 0; m < 3; ++m) many.draw(d, rock.data(), uint32_t(rock.size()), nullptr, 0);
    smooth_shade::recordChanges(many, 640, 480, draws, 4);
    CHECK_EQ(draws, size_t(2));
}
