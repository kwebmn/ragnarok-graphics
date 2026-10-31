// 3.0.11-beta.104 (the owner: «стрелочки очень дергаются при движении»): the stick's walk as the server and the game
// play it - cell by cell (diagonal first, 150 ms straight, 210 ms diagonal; a new ask taken after the step), the
// actor gliding between the cells - and the arrow per 16 ms frame from the controller's way, wall, lead and asked cell
// (core/ground_way glide): against the character its end holds still. beta.103 drew it to the asked cell, which
// hopped a cell on each time the character's cell changed - its end shook by up to a cell a frame
#include <algorithm>
#include <cmath>

#include "core/ground_way.hpp"
#include "pad_fixtures.hpp"
#include "test.hpp"

using namespace fx;

namespace {

struct Walker {
    Cell at{100, 100}, stepTo{100, 100}, dest{100, 100};
    double t = 1;  // the step's progress
    double x() const { return at.x + (stepTo.x - at.x) * std::min(t, 1.0); }
    double y() const { return at.y + (stepTo.y - at.y) * std::min(t, 1.0); }
    void advance(double ms) {
        while (ms > 1e-9) {
            if (t >= 1) {
                at = stepTo;
                if (at == dest) return;
                stepTo = Cell{at.x + (dest.x > at.x) - (dest.x < at.x), at.y + (dest.y > at.y) - (dest.y < at.y)};
                t = 0;
            }
            const double dur = (stepTo.x != at.x && stepTo.y != at.y) ? 210 : 150;
            const double d = std::min(ms, (1 - t) * dur);
            t += d / dur;
            ms -= d;
        }
    }
};

struct Run {
    double worst = 0, wallGap = 1e9;  // cells: the end's most a frame against the feet; the end to the wall's cell
};

Run walk(double degrees, const GroundGrid* ground) {
    Controller c(Config{});
    c.setDirectWalk(true);
    c.setLook(0, 1);
    c.setGround(ground);
    WorldState w = world();
    PadState p = pad();
    p.lx = float(std::cos(degrees * 3.14159265358979 / 180));
    p.ly = float(std::sin(degrees * 3.14159265358979 / 180));
    Walker k;
    uagfx::groundway::WalkGlide gl;
    Run r;
    double beforeX = 0, beforeZ = 0;
    for (uint32_t now = 100; now < 100 + 4000; now += 16) {
        c.setSelfCell(Cell{int(std::lround(k.x())), int(std::lround(k.y()))});
        const TickOutput o = c.tick(p, w, view(), now);
        for (const Bytes& b : o.packets)
            if (pid(b) == 0x00a7) k.dest = decodePos3(b.data(), 5);
        k.advance(16);
        if (!c.walkTarget()) continue;
        uagfx::groundway::WalkAim aim;  // in cells (a cell 1 unit)
        aim.x0 = float(k.x());
        aim.z0 = float(k.y());
        aim.wayX = float(c.walkWay().east);
        aim.wayZ = float(c.walkWay().north);
        aim.cellX = float(c.walkTarget()->x);
        aim.cellZ = float(c.walkTarget()->y);
        aim.cut = c.walkCut();
        aim.lead = float(c.walkLead());
        uagfx::groundway::glide(gl, aim, 16);
        const double ex = std::cos(gl.angle) * gl.len, ez = std::sin(gl.angle) * gl.len;
        if (now > 400) r.worst = std::max(r.worst, std::hypot(ex - beforeX, ez - beforeZ));
        if (aim.cut) r.wallGap = std::hypot(aim.x0 + ex - aim.cellX, aim.z0 + ez - aim.cellZ);
        beforeX = ex;
        beforeZ = ez;
    }
    return r;
}

}  // namespace

TEST(walk_glide_holds_still_with_the_controller) {
    for (const double degrees : {0.0, 30.0, 45.0, 60.0, 135.0}) CHECK(walk(degrees, nullptr).worst < 0.01);
}

TEST(walk_glide_ends_at_a_wall_with_the_controller) {
    GroundGrid g;
    g.w = g.h = 200;
    g.walk.assign(200 * 200, 1);
    for (int y = 0; y < 200; ++y) g.walk[size_t(y) * 200 + 110] = 0;  // a wall at x 110
    const Run r = walk(0, &g);
    CHECK(r.wallGap < 0.1);  // the end on the last cell short of it (109), the character standing there
    CHECK(r.worst < 0.35);   // shortened softly as the wall came within the lead
}
