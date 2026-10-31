#include "win/net_smooth.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/minimap_proto.hpp"
#include "core/net_smooth.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"

namespace uagfx::win::netsmoothwin {
namespace {

namespace ns = uagfx::netsmooth;

// A unit's message (its vtable + 8, thiscall: sender, message, a, b, c) - 0x13 puts it on the cell (a, b), as the
// game's 0x0088 does (the packet loop at 0x579f41 / 0x579f77)
constexpr uint32_t kSendMsgSlot = 8;
constexpr int kPutMsg = 0x13;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct Glide {
    ns::Glide g;
    int cellX = 0, cellY = 0;    // the server's cell: the game told it at the end (its own state then)
    float restX = 0, restZ = 0;  // the middle of the cell the game was told (where it may put the unit back)
    float lastX = 0, lastZ = 0;  // what was written last (the game moving it elsewhere: left to the game)
    bool wrote = false;
};

struct State {
    bool on = true;
    std::vector<Glide> glides;
    int eased = 0, told = 0;
};

State& state() {
    static State s;
    return s;
}

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool samePlace(float a, float b) { return std::fabs(a - b) < 0.01f; }

bool writeFloat(uint32_t at, float v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

void put16(uint8_t* p, size_t at, int v) {
    p[at] = uint8_t(v & 0xFF);
    p[at + 1] = uint8_t((v >> 8) & 0xFF);
}

int rd16(const uint8_t* p, size_t at) { return int(uint16_t(p[at] | p[at + 1] << 8)); }

}  // namespace

void setOn(bool on) {
    State& s = state();
    if (on == s.on) return;
    s.on = on;
    if (!on) s.glides.clear();
    LOG_INFO("net: [interface] smooth_fixes = %d", on ? 1 : 0);
}

int fix(uint32_t id, uint8_t* packet) {
    State& s = state();
    if (!packet) return -1;
    const int x = rd16(packet, 6), y = rd16(packet, 8);
    float world[3] = {}, own[3] = {};
    int w = 0, h = 0, cell = 0;
    minimap::Cell now;
    if (!readActorWorld(id, world) || !readPlayerWorld(own, w, h, cell) ||
        !minimap::cellOf(world[0], world[2], w, h, cell, now))
        return -1;
    const int cells = std::max(std::abs(now.x - x), std::abs(now.y - y));
    if (!s.on || !ns::eases(cells)) return cells;
    // the game told to stop the unit where it stands (no jump), the unit drawn on to the server's cell
    put16(packet, 6, now.x);
    put16(packet, 8, now.y);
    Glide gl;
    gl.g.id = id;
    gl.g.fromX = world[0];
    gl.g.fromZ = world[2];
    ns::cellMiddle(x, y, w, h, cell, gl.g.toX, gl.g.toZ);
    ns::cellMiddle(now.x, now.y, w, h, cell, gl.restX, gl.restZ);
    gl.cellX = x;
    gl.cellY = y;
    gl.g.startMs = GetTickCount64();
    gl.g.durationMs = ns::glideMs(cells);
    s.glides.erase(std::remove_if(s.glides.begin(), s.glides.end(), [&](const Glide& o) { return o.g.id == id; }),
                   s.glides.end());
    s.glides.push_back(gl);
    if (s.told < 12 && ++s.eased % 20 == 1) {
        ++s.told;
        LOG_INFO("net: the server's fix of %u (%d cell%s, to %d, %d) glided to in %u ms, not jumped (%d so far)", id,
                 cells, cells == 1 ? "" : "s", x, y, gl.g.durationMs, s.eased);
    }
    return cells;
}

void beginScene() {
    State& s = state();
    if (s.glides.empty()) return;
    const uint64_t now = GetTickCount64();
    for (auto it = s.glides.begin(); it != s.glides.end();) {
        Glide& gl = *it;
        const uint32_t actor = actorAddress(gl.g.id);
        float at[3] = {};
        if (!actor || !readGame(actor + 4, at, sizeof at)) {  // gone
            it = s.glides.erase(it);
            continue;
        }
        // where we left it or where the game stood it (the cell it was told): still ours; elsewhere - walking on, put
        // there by the game: its own again
        const bool ours = (samePlace(at[0], gl.lastX) && samePlace(at[2], gl.lastZ)) ||
                          (samePlace(at[0], gl.restX) && samePlace(at[2], gl.restZ));
        if (gl.wrote && !ours) {
            it = s.glides.erase(it);
            continue;
        }
        float x = 0, z = 0;
        const bool going = ns::at(gl.g, now, x, z);
        writeFloat(actor + 4, x);
        writeFloat(actor + 12, z);
        gl.lastX = x;
        gl.lastZ = z;
        gl.wrote = true;
        if (!going) {  // there: the game told the server's cell, as its packet would have (its own state the same)
            const uint32_t send = read32(read32(actor) + kSendMsgSlot);
            if (send) reinterpret_cast<SendMsgFn>(uintptr_t(send))(actor, 0, kPutMsg, gl.cellX, gl.cellY, 0);
            it = s.glides.erase(it);
            continue;
        }
        ++it;
    }
}

}  // namespace uagfx::win::netsmoothwin
