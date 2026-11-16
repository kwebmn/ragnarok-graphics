#include "core/room_blocks.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include "core/outskirts.hpp"

namespace uagfx::roomblocks {
namespace {

struct Look {
    int texture = -1;
    int run = 1;             // cells the picture runs over
    float downPerUnit = 0;   // of the picture a unit of the wall's height
    int lightmap = -1;       // the walls' lightmaps evened out: made when first laid (evenedOf)
    std::vector<uint16_t> lit;  // the walls' own lightmaps
    bool known() const { return texture >= 0; }
};

struct Seen {  // a pictured wall face at a room's edge
    int surface;
    float height;
};

float middle(std::vector<float>& v) {
    if (v.empty()) return 0.0f;
    std::nth_element(v.begin(), v.begin() + ptrdiff_t(v.size() / 2), v.end());
    return v[v.size() / 2];
}

Look lookOf(const gnd::Ground& g, const std::map<int, std::vector<Seen>>& byTexture) {
    Look look;
    float most = 0.0f;  // by the faces' height together: the wall's, not a rim's
    for (const auto& [texture, faces] : byTexture) {
        float tall = 0.0f;
        for (const Seen& f : faces) tall += f.height;
        if (tall > most) most = tall, look.texture = texture;
    }
    if (!look.known()) return look;
    const std::vector<Seen>& faces = byTexture.at(look.texture);
    std::vector<float> across, down;
    for (const Seen& f : faces) {
        const gnd::Surface& s = g.surfaces[size_t(f.surface)];
        const float du = std::fabs(s.u[1] - s.u[0]), dv = std::fabs(s.v[2] - s.v[0]);
        if (du > 1e-4f) across.push_back(du);
        if (f.height > 1.0f && dv > 1e-4f) down.push_back(dv / f.height);
        if (size_t(s.lightmap) < g.lightmaps.size()) look.lit.push_back(s.lightmap);
    }
    const float du = middle(across);
    look.run = du > 1e-4f ? std::clamp(int(std::lround(1.0f / du)), 1, kMostRun) : 1;
    look.downPerUnit = middle(down);
    if (!(look.downPerUnit > 0.0f)) look.downPerUnit = 1.0f / 30.0f;  // a wall of thirty: the whole picture
    return look;
}

// the look's lightmap: its walls' own brightness, each texel their mean - added to the ground's once. No colour of
// its own (a lightmap's colour is added to the picture): from above these faces show the makers' black, and black
// must stay black there
uint16_t evenedOf(gnd::Ground& g, Look& look) {
    if (look.lightmap >= 0) return uint16_t(look.lightmap);
    gnd::Lightmap mean{};
    std::fill(mean.brightness, mean.brightness + 64, uint8_t(255));
    if (!look.lit.empty()) {
        uint32_t bright[64] = {};
        for (uint16_t id : look.lit) {
            const gnd::Lightmap& one = g.lightmaps[id];
            for (int i = 0; i < 64; ++i) bright[i] += one.brightness[i];
        }
        const uint32_t n = uint32_t(look.lit.size());
        for (int i = 0; i < 64; ++i) mean.brightness[i] = uint8_t((bright[i] + n / 2) / n);
    }
    g.lightmaps.push_back(mean);
    look.lightmap = int(g.lightmaps.size() - 1);
    return uint16_t(look.lightmap);
}

}  // namespace

Done dress(gnd::Ground& g, const rooms::Map& rooms) {
    Done done;
    const int w = g.width, h = g.height;
    if (w <= 0 || h <= 0 || g.cells.size() < size_t(w) * size_t(h) || rooms.width != w || rooms.height != h ||
        rooms.room.size() < size_t(w) * size_t(h) || rooms.count <= 0)
        return done;
    const auto cell = [&](int x, int y) -> gnd::Cell& { return g.cells[size_t(y) * size_t(w) + size_t(x)]; };
    const auto roomOf = [&](int x, int y) { return int(rooms.room[size_t(y) * size_t(w) + size_t(x)]); };
    const auto textureOf = [&](int32_t surface) {
        if (surface < 0 || size_t(surface) >= g.surfaces.size()) return -1;
        const int t = g.surfaces[size_t(surface)].texture;
        return t >= 0 && size_t(t) < g.textures.size() ? t : -1;
    };
    const auto filler = [&](int32_t surface) {
        const int t = textureOf(surface);
        return t >= 0 && outskirts::fillerTexture(g.textures[size_t(t)]);
    };
    const auto pictured = [&](int32_t surface) { return textureOf(surface) >= 0 && !filler(surface); };
    const auto up = [&](int x, int y) {
        const gnd::Cell& c = cell(x, y);
        return -(c.h[0] + c.h[1] + c.h[2] + c.h[3]) / 4.0f;
    };
    // a side of the cell (x, y): 0 towards x + 1, 1 towards x - 1, 2 towards y + 1, 3 towards y - 1 - the cell that
    // owns its face (its right or its front), and how far up each of the two stands along that edge
    struct Side {
        bool there = false, right = false, ownerIsThis = false;
        int ox = 0, oy = 0, nx = 0, ny = 0;
        float thisUp = 0, otherUp = 0;
    };
    const auto sideOf = [&](int x, int y, int k) {
        Side s;
        s.nx = x + (k == 0 ? 1 : k == 1 ? -1 : 0);
        s.ny = y + (k == 2 ? 1 : k == 3 ? -1 : 0);
        if (s.nx < 0 || s.ny < 0 || s.nx >= w || s.ny >= h) return s;
        s.there = true;
        s.right = k < 2;
        s.ownerIsThis = k == 0 || k == 2;
        s.ox = s.ownerIsThis ? x : s.nx;
        s.oy = s.ownerIsThis ? y : s.ny;
        const gnd::Cell& c = cell(x, y);
        const gnd::Cell& n = cell(s.nx, s.ny);
        // the corners along the shared edge (gnd::Surface's order: (x, y), (x + 1, y), (x, y + 1), (x + 1, y + 1))
        static const int kEdge[4][2] = {{1, 3}, {0, 2}, {2, 3}, {0, 1}};       // this cell's
        static const int kEdgeOther[4][2] = {{0, 2}, {1, 3}, {0, 1}, {2, 3}};  // the neighbour's
        s.thisUp = -(c.h[kEdge[k][0]] + c.h[kEdge[k][1]]) / 2.0f;
        s.otherUp = -(n.h[kEdgeOther[k][0]] + n.h[kEdgeOther[k][1]]) / 2.0f;
        return s;
    };
    const auto faceOf = [&](const Side& s) -> int32_t& {
        gnd::Cell& owner = cell(s.ox, s.oy);
        return s.right ? owner.right : owner.front;
    };

    // each room's wall: the pictured faces along its edges that stand against a block (the filler raised beside the
    // floor) - a step's or a platform's rim in the floor's tiles is no wall
    std::vector<std::map<int, std::vector<Seen>>> seen(size_t(rooms.count) + 1);
    std::map<int, std::vector<Seen>> all;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int r = roomOf(x, y);
            if (r <= 0 || r > rooms.count) continue;
            for (int k = 0; k < 4; ++k) {
                const Side s = sideOf(x, y, k);
                if (!s.there || roomOf(s.nx, s.ny) == r) continue;
                const int32_t face = faceOf(s);
                if (!pictured(face) || !filler(cell(s.nx, s.ny).top) || !(s.otherUp - s.thisUp > kBlockRise)) continue;
                const Seen one{int(face), s.otherUp - s.thisUp};
                seen[size_t(r)][textureOf(face)].push_back(one);
                all[textureOf(face)].push_back(one);
            }
        }
    Look common = lookOf(g, all);
    if (!common.known() || g.lightmaps.size() + size_t(rooms.count) + 1 > 0xFFFF ||
        g.textures.size() + 64 > 0x7FFF)
        return done;
    std::vector<Look> looks(size_t(rooms.count) + 1);
    for (int r = 1; r <= rooms.count; ++r) looks[size_t(r)] = lookOf(g, seen[size_t(r)]);

    // the blocks: filler tops by a room, over its floor
    std::vector<uint8_t> block(size_t(w) * size_t(h), 0);  // the room whose look it takes; 0 none
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (roomOf(x, y) != 0 || !filler(cell(x, y).top)) continue;
            int room = 0;
            float floor = 0.0f;
            for (int reach = 1; reach <= kReach && !room; ++reach)
                for (int dy = -reach; dy <= reach && !room; ++dy)
                    for (int dx = -reach; dx <= reach && !room; ++dx) {
                        if (std::max(std::abs(dx), std::abs(dy)) != reach) continue;
                        const int px = x + dx, py = y + dy;
                        if (px < 0 || py < 0 || px >= w || py >= h) continue;
                        const int r = roomOf(px, py);
                        if (r > 0 && r <= rooms.count) room = r, floor = up(px, py);
                    }
            if (room && up(x, y) - floor > kBlockRise) block[size_t(y) * size_t(w) + size_t(x)] = uint8_t(room);
        }

    std::vector<uint8_t> roomDone(size_t(rooms.count) + 1, 0);
    const auto runAt = [](int at, int run) { return float(((at % run) + run) % run) / float(run); };
    // the wall's texture again for the dressed faces: one for each filler it takes the place of (kNotDrawn: of none)
    const size_t ownTextures = g.textures.size();
    std::map<std::pair<int, int>, int> copies;
    std::vector<int> shown;  // the copies' fromAbove, in their order
    const auto copyOf = [&](int texture, int was) {
        const auto key = std::make_pair(texture, was);
        auto it = copies.find(key);
        if (it == copies.end()) {
            g.textures.push_back(g.textures[size_t(texture)]);
            shown.push_back(was);
            it = copies.emplace(key, int(g.textures.size() - 1)).first;
        }
        return int16_t(it->second);
    };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int room = block[size_t(y) * size_t(w) + size_t(x)];
            if (!room) continue;
            Look& look = looks[size_t(room)].known() ? looks[size_t(room)] : common;  // a room of no wall: the map's
            const uint16_t lightmap = evenedOf(g, look);
            const float share = 1.0f / float(look.run);
            // its top: the picture's upper band (its rim) over the cells across
            gnd::Surface top;
            const float u0 = runAt(x, look.run);
            const float tu[4] = {u0, u0 + share, u0, u0 + share}, tv[4] = {0.0f, 0.0f, kCapShare, kCapShare};
            std::copy(tu, tu + 4, top.u);
            std::copy(tv, tv + 4, top.v);
            top.texture = copyOf(look.texture, textureOf(cell(x, y).top));
            top.lightmap = lightmap;
            top.color = 0xFFFFFFFFu;
            g.surfaces.push_back(top);
            cell(x, y).top = int32_t(g.surfaces.size() - 1);
            ++done.tops;
            roomDone[size_t(room)] = 1;
            // its sides down to what stands lower: the makers' own picture kept, their black dressed, none made
            for (int k = 0; k < 4; ++k) {
                const Side s = sideOf(x, y, k);
                if (!s.there || !(s.thisUp - s.otherUp > 0.5f)) continue;
                int32_t& face = faceOf(s);
                if (pictured(face)) continue;
                const bool black = filler(face);  // else the makers left no face there
                gnd::Surface side;
                const float down = std::min(1.0f, (s.thisUp - s.otherUp) * look.downPerUnit);
                // across: along z for a right face (its corners 0 and 2 at the far end, 1 and 3 at the near one),
                // along x for a front face (0 and 2 at x, 1 and 3 at x + 1)
                const float a = runAt(s.right ? y : x, look.run);
                const float su[4] = {s.right ? a + share : a, s.right ? a : a + share, s.right ? a + share : a,
                                     s.right ? a : a + share};
                // down: corners 0 and 1 on the owner's edge, 2 and 3 on the other cell's; the picture's top on the
                // block's edge
                const float atOwner = s.ownerIsThis ? 0.0f : down, atOther = s.ownerIsThis ? down : 0.0f;
                const float sv[4] = {atOwner, atOwner, atOther, atOther};
                std::copy(su, su + 4, side.u);
                std::copy(sv, sv + 4, side.v);
                side.texture = copyOf(look.texture, black ? textureOf(face) : kNotDrawn);
                side.lightmap = lightmap;
                side.color = 0xFFFFFFFFu;
                g.surfaces.push_back(side);
                face = int32_t(g.surfaces.size() - 1);
                ++(black ? done.sides : done.made);
            }
        }
    for (uint8_t d : roomDone) done.rooms += d;
    done.fromAbove.assign(ownTextures, kAsItIs);
    done.fromAbove.insert(done.fromAbove.end(), shown.begin(), shown.end());
    return done;
}

}  // namespace uagfx::roomblocks
