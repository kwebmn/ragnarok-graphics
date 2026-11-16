// 3.0.20-beta.56 (R-2BX4BG, the owner in pay_dun00's 3D view: «и там скала прозрачная какая-то»): the cave's «mushroom
// rock» (외부소품\\버섯바위.rsm) is, in its own file, a cap hung 3.9 over an open-topped base with nothing between - a
// model made for the view from above, where the cap hides the gap; from the eyes' level the wall behind shows through
// it. Of some 2800 model files 74 have such a part under another (/root/uagfx-tools/audit/models/hatcensus.cpp: a
// pagoda's tiers, a guard post's walls under its roof). For the 3D view the gap is closed: the lower part's open top
// raised to the part over it, that part's open bottom given a lid
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <vector>

#include "core/config.hpp"
#include "core/game_models.hpp"
#include "core/live_config.hpp"
#include "core/model_gaps.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// 외부소품\버섯바위.rsm as its file has it (/root/uagfx-tools/audit/sunshadow/rsmgap): a face a row - its texture, then
// its three corners' x, height (up positive) and z
const float kMushroom[30][10] = {
    {0, 22.2f, 23.2f, 0.0f, 7.4f, 30.5f, 12.9f, 14.9f, 30.5f, 0.0f},
    {0, 22.2f, 23.2f, 0.0f, 11.1f, 23.2f, 19.2f, 7.4f, 30.5f, 12.9f},
    {0, 11.1f, 23.2f, 19.2f, -7.4f, 30.5f, 12.9f, 7.4f, 30.5f, 12.9f},
    {0, 11.1f, 23.2f, 19.2f, -11.1f, 23.2f, 19.2f, -7.4f, 30.5f, 12.9f},
    {0, -11.1f, 23.2f, 19.2f, -14.9f, 30.5f, 0.0f, -7.4f, 30.5f, 12.9f},
    {0, -11.1f, 23.2f, 19.2f, -22.2f, 23.2f, 0.0f, -14.9f, 30.5f, 0.0f},
    {0, -22.2f, 23.2f, 0.0f, -7.4f, 30.5f, -12.9f, -14.9f, 30.5f, 0.0f},
    {0, -22.2f, 23.2f, 0.0f, -11.1f, 23.2f, -19.2f, -7.4f, 30.5f, -12.9f},
    {0, 11.1f, 23.2f, -19.2f, 14.9f, 30.5f, 0.0f, 7.4f, 30.5f, -12.9f},
    {0, 11.1f, 23.2f, -19.2f, 22.2f, 23.2f, 0.0f, 14.9f, 30.5f, 0.0f},
    {0, 14.9f, 30.5f, 0.0f, 7.4f, 30.5f, 12.9f, 0.0f, 37.8f, 0.0f},
    {0, 7.4f, 30.5f, 12.9f, -7.4f, 30.5f, 12.9f, 0.0f, 37.8f, 0.0f},
    {0, -7.4f, 30.5f, 12.9f, -14.9f, 30.5f, 0.0f, 0.0f, 37.8f, 0.0f},
    {0, -14.9f, 30.5f, 0.0f, -7.4f, 30.5f, -12.9f, 0.0f, 37.8f, 0.0f},
    {0, 7.4f, 30.5f, -12.9f, 14.9f, 30.5f, 0.0f, 0.0f, 37.8f, 0.0f},
    {0, -11.1f, 23.2f, -19.2f, 7.4f, 30.5f, -12.9f, -7.4f, 30.5f, -12.9f},
    {0, -11.1f, 23.2f, -19.2f, 11.1f, 23.2f, -19.2f, 7.4f, 30.5f, -12.9f},
    {0, -7.4f, 30.5f, -12.9f, 7.4f, 30.5f, -12.9f, 0.0f, 37.8f, 0.0f},
    {1, 15.7f, 0.0f, 0.0f, 4.4f, 19.3f, 7.5f, 8.7f, 19.3f, 0.0f},
    {1, 15.7f, 0.0f, 0.0f, 7.9f, 0.0f, 13.5f, 4.4f, 19.3f, 7.5f},
    {1, 7.9f, 0.0f, 13.5f, -4.2f, 19.3f, 7.5f, 4.4f, 19.3f, 7.5f},
    {1, 7.9f, 0.0f, 13.5f, -7.7f, 0.0f, 13.5f, -4.2f, 19.3f, 7.5f},
    {1, -7.7f, 0.0f, 13.5f, -8.5f, 19.3f, 0.0f, -4.2f, 19.3f, 7.5f},
    {1, -7.7f, 0.0f, 13.5f, -15.5f, 0.0f, 0.0f, -8.5f, 19.3f, 0.0f},
    {1, -15.5f, 0.0f, 0.0f, -4.2f, 19.3f, -7.5f, -8.5f, 19.3f, 0.0f},
    {1, -15.5f, 0.0f, 0.0f, -7.7f, 0.0f, -13.5f, -4.2f, 19.3f, -7.5f},
    {1, -7.7f, 0.0f, -13.5f, 4.4f, 19.3f, -7.5f, -4.2f, 19.3f, -7.5f},
    {1, -7.7f, 0.0f, -13.5f, 7.9f, 0.0f, -13.5f, 4.4f, 19.3f, -7.5f},
    {1, 7.9f, 0.0f, -13.5f, 8.7f, 19.3f, 0.0f, 4.4f, 19.3f, -7.5f},
    {1, 7.9f, 0.0f, -13.5f, 15.7f, 0.0f, 0.0f, 8.7f, 19.3f, 0.0f},
};

// a node of faces given as rows of a texture and three corners (x, height, z): the corners of one place one vertex,
// the heights turned into the world's y (up along -y)
gamemodels::Node nodeOf(const float (*rows)[10], size_t count) {
    gamemodels::Node n;
    n.id = 0x1000;
    n.textures = {11, 12};
    std::map<std::tuple<int, int, int>, uint16_t> at;
    for (size_t f = 0; f < count; ++f) {
        gamemodels::Face face;
        face.texture = uint8_t(rows[f][0]);
        for (int k = 0; k < 3; ++k) {
            const float* p = rows[f] + 1 + 3 * k;
            const auto key = std::make_tuple(int(std::lround(p[0] * 16)), int(std::lround(p[1] * 16)),
                                             int(std::lround(p[2] * 16)));
            auto it = at.find(key);
            if (it == at.end()) {
                it = at.emplace(key, uint16_t(n.vertices.size() / 3)).first;
                n.vertices.insert(n.vertices.end(), {p[0], -p[1], p[2]});
            }
            face.v[k] = it->second;
            face.uv[2 * k] = 0.1f * float(k) + 0.01f * float(f);
            face.uv[2 * k + 1] = 0.5f;
            face.colour[k] = 0xFF000000u | uint32_t(f * 3 + size_t(k));
        }
        n.faces.push_back(face);
    }
    return n;
}

// whether the way from `from` to `to` (x, height, z) meets a face of the node
bool meets(const gamemodels::Node& n, const float from[3], const float to[3]) {
    const float o[3] = {from[0], -from[1], from[2]};
    const float d[3] = {to[0] - from[0], -(to[1] - from[1]), to[2] - from[2]};
    for (const gamemodels::Face& f : n.faces) {
        const float* a = &n.vertices[size_t(f.v[0]) * 3];
        const float* b = &n.vertices[size_t(f.v[1]) * 3];
        const float* c = &n.vertices[size_t(f.v[2]) * 3];
        const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float p[3] = {d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0]};
        const float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
        if (std::fabs(det) < 1e-7f) continue;
        const float t0[3] = {o[0] - a[0], o[1] - a[1], o[2] - a[2]};
        const float u = (t0[0] * p[0] + t0[1] * p[1] + t0[2] * p[2]) / det;
        const float q[3] = {t0[1] * e1[2] - t0[2] * e1[1], t0[2] * e1[0] - t0[0] * e1[2], t0[0] * e1[1] - t0[1] * e1[0]};
        const float v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) / det;
        const float t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det;
        if (u >= 0.0f && v >= 0.0f && u + v <= 1.0f && t >= 0.0f && t <= 1.0f) return true;
    }
    return false;
}

// a box's four walls from `low` to `high` about the middle, `half` each way, with a lid or without (8 or 10 faces),
// and a floor under it (2 more)
void box(std::vector<std::array<float, 10>>& rows, float half, float low, float high, bool lid, float texture = 0,
         bool floored = false) {
    const float c[4][2] = {{-half, -half}, {half, -half}, {half, half}, {-half, half}};
    for (int k = 0; k < 4; ++k) {
        const float* a = c[k];
        const float* b = c[(k + 1) % 4];
        rows.push_back({texture, a[0], low, a[1], b[0], low, b[1], b[0], high, b[1]});
        rows.push_back({texture, a[0], low, a[1], b[0], high, b[1], a[0], high, a[1]});
    }
    if (floored) {
        rows.push_back({texture, c[0][0], low, c[0][1], c[1][0], low, c[1][1], c[2][0], low, c[2][1]});
        rows.push_back({texture, c[0][0], low, c[0][1], c[2][0], low, c[2][1], c[3][0], low, c[3][1]});
    }
    if (!lid) return;
    rows.push_back({texture, c[0][0], high, c[0][1], c[1][0], high, c[1][1], c[2][0], high, c[2][1]});
    rows.push_back({texture, c[0][0], high, c[0][1], c[2][0], high, c[2][1], c[3][0], high, c[3][1]});
}

gamemodels::Node nodeOf(const std::vector<std::array<float, 10>>& rows) {
    std::vector<float> flat;
    for (const auto& r : rows) flat.insert(flat.end(), r.begin(), r.end());
    return nodeOf(reinterpret_cast<const float (*)[10]>(flat.data()), rows.size());
}

}  // namespace

TEST(model_gaps_the_mushroom_rocks_cap_joined_to_its_base) {
    gamemodels::Node rock = nodeOf(kMushroom, 30);
    const gamemodels::Node before = rock;
    // through the gap from side to side (at 21.3 of its height - between the base's top 19.3 and the cap's rim
    // 23.2), and up into the open cap from under its rim: nothing in the way as the file has it
    const float left[3] = {-40.0f, 21.3f, 0.5f}, right[3] = {40.0f, 21.3f, 0.5f};
    const float under[3] = {18.0f, 20.0f, 1.0f}, inside[3] = {-10.0f, 29.0f, 1.0f};
    CHECK(!meets(rock, left, right) && !meets(rock, under, inside));
    const size_t added = model_gaps::close(rock);
    // the base's ring of six edges raised (two faces each), the cap's open bottom of six edges given a lid
    CHECK(added == 18 && rock.faces.size() == 48);
    // told apart from the game's own: its last 18 faces, its last 13 corners (two a wall's edge... one a ring's
    // corner would do; the lid's middle one more)
    CHECK(rock.addedFaces == 18 && rock.addedVertices == rock.vertices.size() / 3 - before.vertices.size() / 3);
    CHECK(meets(rock, left, right));     // the stem
    CHECK(meets(rock, under, inside));   // the cap's underside
    // what the file has stays as it is; the new faces drawn from both sides, of their parts' own textures
    for (size_t f = 0; f < 30; ++f) {
        CHECK(!std::memcmp(rock.faces[f].v, before.faces[f].v, sizeof before.faces[f].v));
        CHECK(rock.faces[f].flags == before.faces[f].flags);
    }
    size_t ofBase = 0, ofCap = 0;
    for (size_t f = 30; f < rock.faces.size(); ++f) {
        CHECK((rock.faces[f].flags & gamemodels::kTwoSided) != 0);
        ofBase += rock.faces[f].texture == 1;
        ofCap += rock.faces[f].texture == 0;
    }
    CHECK(ofBase == 12 && ofCap == 6);
    // the new corners: between the base's top and the cap's rim, within the cap's reach
    for (size_t v = before.vertices.size(); v + 2 < rock.vertices.size(); v += 3) {
        CHECK(-rock.vertices[v + 1] >= 19.2f && -rock.vertices[v + 1] <= 23.3f);
        CHECK(std::hypot(rock.vertices[v], rock.vertices[v + 2]) <= 22.3f);
    }
    // a corner of a new face wears what the corner under it wears (the wall's colours and picture drawn on up)
    const gamemodels::Face& wall = rock.faces[30];
    CHECK(wall.colour[0] != 0 && wall.colour[1] != 0 && wall.colour[2] != 0);
    CHECK(model_gaps::close(rock) == 0);  // closed: nothing more to close
}

TEST(model_gaps_only_an_open_top_wide_for_its_gap_under_another_part) {
    // a base with its own lid under a slab: its makers meant the gap (a lantern over its stand)
    std::vector<std::array<float, 10>> lidded;
    box(lidded, 8.0f, 0.0f, 10.0f, true, 1);
    box(lidded, 12.0f, 14.0f, 16.0f, true, 0);
    gamemodels::Node a = nodeOf(lidded);
    CHECK(model_gaps::close(a) == 0);
    // the same base open on top: closed - four edges' walls up to the slab's underside (it has one: no lid needed)
    std::vector<std::array<float, 10>> open;
    box(open, 8.0f, 0.0f, 10.0f, false, 1);
    box(open, 12.0f, 14.0f, 16.0f, true, 0, true);
    gamemodels::Node b = nodeOf(open);
    CHECK(model_gaps::close(b) == 8);
    const float from[3] = {-30.0f, 12.0f, 0.5f}, to[3] = {30.0f, 12.0f, 0.5f};
    CHECK(meets(b, from, to));
    // a slab open below (its four sides and its top alone): a lid of four faces too
    std::vector<std::array<float, 10>> hollow;
    box(hollow, 8.0f, 0.0f, 10.0f, false, 1);
    box(hollow, 12.0f, 14.0f, 16.0f, true, 0);
    gamemodels::Node hollowed = nodeOf(hollow);
    CHECK(model_gaps::close(hollowed) == 12);
    // a table's leg, 2 across, 2 under its top: narrower than kWide of its gap - left as its makers drew it
    std::vector<std::array<float, 10>> leg;
    box(leg, 1.0f, 0.0f, 8.0f, false, 1);
    box(leg, 12.0f, 10.0f, 11.0f, true, 0, true);
    gamemodels::Node c = nodeOf(leg);
    CHECK(model_gaps::close(c) == 0);
    // a roof far over a wall (past kMostGap): a pavilion's open side stays open
    std::vector<std::array<float, 10>> pavilion;
    box(pavilion, 20.0f, 0.0f, 6.0f, false, 1);
    box(pavilion, 24.0f, 6.0f + model_gaps::kMostGap + 1.0f, 30.0f, true, 0, true);
    gamemodels::Node d = nodeOf(pavilion);
    CHECK(model_gaps::close(d) == 0);
    // nothing over it; a part alone; a moving node (its own space, the game moves it); nothing at all
    std::vector<std::array<float, 10>> alone;
    box(alone, 8.0f, 0.0f, 10.0f, false, 1);
    gamemodels::Node e = nodeOf(alone);
    CHECK(model_gaps::close(e) == 0);
    gamemodels::Node moving = nodeOf(hollow);
    moving.moving = true;
    CHECK(model_gaps::close(moving) == 0);
    gamemodels::Node none;
    CHECK(model_gaps::close(none) == 0);
}

TEST(config_view3d_close_gaps_on_by_default) {
    CHECK(parseConfig("").effects.closeGaps);
    CHECK(!parseConfig("[view3d]\nclose_gaps = 0\n").effects.closeGaps);
    const settings::IniSetting* row = settings::findSetting("view3d.close_gaps");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Game && row->control == settings::Control::Check);
    CHECK(appliesNow("view3d.close_gaps"));
}
