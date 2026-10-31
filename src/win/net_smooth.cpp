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

struct Pending {  // 3.0.11-beta.124: a move seen - where the unit was shown before the game took it
    uint32_t id = 0;
    float x = 0, z = 0;
};
struct Fade {  // a move's jump fading out over the game's own place
    ns::Offset o;
    float lastX = 0, lastZ = 0, baseX = 0, baseZ = 0;  // written last, the game's place under it
};

struct State {
    bool on = true;
    std::vector<Glide> glides;
    std::vector<Pending> pending;
    std::vector<Fade> fades;
    int eased = 0, told = 0, faded = 0, fadesTold = 0;
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
    if (!on) {
        s.glides.clear();
        s.pending.clear();
        s.fades.clear();
    }
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

void moveSeen(uint32_t id) {
    State& s = state();
    float at[3] = {};
    if (!s.on || !id || !readActorWorld(id, at)) return;
    s.pending.erase(std::remove_if(s.pending.begin(), s.pending.end(), [&](const Pending& p) { return p.id == id; }),
                    s.pending.end());
    s.pending.push_back(Pending{id, at[0], at[2]});
}

namespace {

// The moves taken this frame: the game's place now against where the unit was shown - a jump faded out
void startFades(State& s, uint64_t now) {
    float own[3] = {};
    int w = 0, h = 0, cell = 0;
    if (s.pending.empty() || !readPlayerWorld(own, w, h, cell) || cell <= 0) {
        s.pending.clear();
        return;
    }
    for (const Pending& p : s.pending) {
        float at[3] = {};
        if (!readActorWorld(p.id, at)) continue;
        const float dx = p.x - at[0], dz = p.z - at[2];
        const float cells = std::max(std::fabs(dx), std::fabs(dz)) / float(cell);
        if (!ns::easesJump(cells)) continue;
        Fade f;
        f.o.id = p.id;
        f.o.offX = dx;
        f.o.offZ = dz;
        f.o.startMs = now;
        f.o.durationMs = uint32_t(100 + 45 * std::min(cells, 4.0f));
        s.fades.erase(std::remove_if(s.fades.begin(), s.fades.end(), [&](const Fade& o) { return o.o.id == p.id; }),
                      s.fades.end());
        s.fades.push_back(f);
        if (s.fadesTold < 12 && ++s.faded % 25 == 1) {
            ++s.fadesTold;
            LOG_INFO("net: a move of %u jumped %.1f cells (late or from another cell) - drawn away over %u ms (%d so "
                     "far)", p.id, double(cells), f.o.durationMs, s.faded);
        }
    }
    s.pending.clear();
}

void fadeFrame(State& s, uint64_t now) {
    for (auto it = s.fades.begin(); it != s.fades.end();) {
        Fade& f = *it;
        const uint32_t actor = actorAddress(f.o.id);
        float at[3] = {};
        if (!actor || !readGame(actor + 4, at, sizeof at)) {
            it = s.fades.erase(it);
            continue;
        }
        // the game's place: where it put it this frame - or, not moved since we wrote (stopped), the one under that
        const bool unmoved = f.o.startMs != now && samePlace(at[0], f.lastX) && samePlace(at[2], f.lastZ);
        if (!unmoved) {
            f.baseX = at[0];
            f.baseZ = at[2];
        }
        float ox = 0, oz = 0;
        const bool going = ns::offsetAt(f.o, now, ox, oz);
        f.lastX = f.baseX + ox;
        f.lastZ = f.baseZ + oz;
        writeFloat(actor + 4, f.lastX);
        writeFloat(actor + 12, f.lastZ);
        if (!going) {
            it = s.fades.erase(it);
            continue;
        }
        ++it;
    }
}

}  // namespace

void beginScene() {
    State& s = state();
    const uint64_t now = GetTickCount64();
    startFades(s, now);  // 3.0.11-beta.124
    fadeFrame(s, now);
    if (s.glides.empty()) return;
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
