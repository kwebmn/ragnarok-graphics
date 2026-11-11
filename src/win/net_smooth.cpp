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
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/net_watch.hpp"
#include "win/safe_read.hpp"

namespace uagfx::win::netsmoothwin {
namespace {

namespace ns = uagfx::netsmooth;

// A unit's message (its vtable + 8, thiscall: sender, message, a, b, c) - 0x13 puts it on the cell (a, b), as the
// game's 0x0088 does (the packet loop at 0x579f41 / 0x579f77)
constexpr uint32_t kSendMsgSlot = 8;
constexpr int kPutMsg = 0x13;
constexpr int kWalkMsg = 0;  // 3.0.11-beta.251: a walk - <tick, from x, y, sub x, sub y, to x, y> (FUN_0057e1f0's)
constexpr uint32_t kOwnAccount = 0x771F7C;

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
    bool distant = false;  // 3.0.11-beta.251: a stranded unit put back - up to 12 cells drawn away
};
// 3.0.11-beta.251: the walk or stop the server gave a unit last, and how long the game has shown it standing
struct Kept {
    uint32_t id = 0;
    bool walking = false;
    uint32_t block[7] = {};  // the game's walk message: tick, from x, y, sub x, sub y, to x, y
    int toX = 0, toY = 0;
    uint64_t seenMs = 0, stillSince = 0;
    float lastX = 0, lastZ = 0;
    int resent = 0;
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
    std::vector<Kept> kept;  // 3.0.11-beta.251
    int strandedTold = 0, stranded = 0;
    int eased = 0, told = 0, faded = 0, fadesTold = 0;
    int leftTold = 0;  // 3.0.11-beta.248 (R-AMNV99): glides the game took over before their end
};

State& state() {
    static State s;
    return s;
}

bool samePlace(float a, float b) { return std::fabs(a - b) < 0.01f; }

Kept& keep(State& s, uint32_t id) {
    for (Kept& k : s.kept)
        if (k.id == id) return k;
    if (s.kept.size() >= 64) s.kept.erase(s.kept.begin());
    s.kept.push_back(Kept{});
    s.kept.back().id = id;
    return s.kept.back();
}

bool writeFloat(uint32_t at, float v) {  // 3.0.18-beta.166: a safe store (win/safe_read) - no call into Windows
    if (saferead::ready()) return saferead::put(at, &v, 4);
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
    if (s.on && id != read32(kOwnAccount)) {  // 3.0.11-beta.251: where the server stopped it
        Kept& k = keep(s, id);
        k.walking = false;
        k.toX = x;
        k.toY = y;
        k.seenMs = GetTickCount64();
        k.stillSince = 0;
        k.resent = 0;
    }
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
        if (!(p.distant ? ns::easesFar(cells) : ns::easesJump(cells))) continue;
        Fade f;
        f.o.id = p.id;
        f.o.offX = dx;
        f.o.offZ = dz;
        f.o.startMs = now;
        f.o.durationMs = p.distant ? ns::farFadeMs(cells) : uint32_t(100 + 45 * std::min(cells, 4.0f));
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

void walkSeen(uint32_t id, const uint8_t* p) {
    State& s = state();
    if (!s.on || !id || !p || id == read32(kOwnAccount)) return;
    Kept& k = keep(s, id);
    k.walking = true;
    uint32_t tick = 0;
    std::memcpy(&tick, p + 12, 4);
    const uint8_t* w = p + 6;  // FUN_0057d420's way
    k.block[0] = tick;
    k.block[1] = uint32_t((w[0] << 2) | (w[1] >> 6));
    k.block[2] = uint32_t(((w[1] & 0x3F) << 4) | (w[2] >> 4));
    k.block[3] = uint32_t(w[5] >> 4);
    k.block[4] = uint32_t(w[5] & 0x0F);
    k.block[5] = uint32_t(((w[2] & 0x0F) << 6) | (w[3] >> 2));
    k.block[6] = uint32_t(((w[3] & 0x03) << 8) | w[4]);
    k.toX = int(k.block[5]);
    k.toY = int(k.block[6]);
    k.seenMs = GetTickCount64();
    k.stillSince = 0;
    k.resent = 0;
}

void gone(uint32_t id) {
    State& s = state();
    s.kept.erase(std::remove_if(s.kept.begin(), s.kept.end(), [&](const Kept& k) { return k.id == id; }),
                 s.kept.end());
}

namespace {

// 3.0.11-beta.251: the units the game left standing off their server's cell - their walk (or a walk to the stop's
// cell, long past: the game puts them there) given again, the jump drawn away
void strandFrame(State& s, uint64_t now) {
    if (s.kept.empty()) return;
    float own[3] = {};
    int w = 0, h = 0, cell = 0;
    if (!readPlayerWorld(own, w, h, cell) || cell <= 0) return;
    for (auto it = s.kept.begin(); it != s.kept.end();) {
        Kept& k = *it;
        const uint32_t actor = actorAddress(k.id);
        float at[3] = {};
        if (!actor || now - k.seenMs > 15000 || !readGame(actor + 4, at, sizeof at)) {
            it = s.kept.erase(it);
            continue;
        }
        ++it;
        const bool gliding =
            std::any_of(s.glides.begin(), s.glides.end(), [&](const Glide& g) { return g.g.id == k.id; });
        if (gliding || !k.stillSince || !samePlace(at[0], k.lastX) || !samePlace(at[2], k.lastZ)) {
            k.lastX = at[0];
            k.lastZ = at[2];
            k.stillSince = now;
            continue;
        }
        minimap::Cell shown;
        if (k.resent >= ns::kResendMost || now - k.seenMs < ns::kStrandMs ||
            !minimap::cellOf(at[0], at[2], w, h, cell, shown) ||
            !ns::stranded(shown.x, shown.y, k.toX, k.toY, now - k.stillSince))
            continue;
        uint32_t block[7];
        if (k.walking) {
            std::memcpy(block, k.block, sizeof block);  // its own tick: the game walks it on from where it is by now
        } else {
            const uint32_t clock = netwatchwin::serverNow();
            block[0] = clock - 60000;  // long past: the game puts it on the cell
            block[1] = uint32_t(shown.x);
            block[2] = uint32_t(shown.y);
            block[3] = block[4] = 8;
            block[5] = uint32_t(k.toX);
            block[6] = uint32_t(k.toY);
        }
        const uint32_t send = read32(read32(actor) + kSendMsgSlot);
        if (!gameObject(actor) || !gameCode(send)) continue;  // .272 (R-EP778D): never a freed actor's
        const auto sameUnit = [&](const Pending& p) { return p.id == k.id; };
        s.pending.erase(std::remove_if(s.pending.begin(), s.pending.end(), sameUnit), s.pending.end());
        s.pending.push_back(Pending{k.id, at[0], at[2], true});
        reinterpret_cast<SendMsgFn>(uintptr_t(send))(actor, 0, kWalkMsg, int(uintptr_t(block)), 0, 0);
        ++k.resent;
        k.stillSince = now;
        if (s.strandedTold < 20) {
            ++s.strandedTold;
            LOG_INFO("net: %u left standing by the game at %d, %d - %s %d, %d given again (%d so far)", k.id, shown.x,
                     shown.y, k.walking ? "its walk to" : "the server's stop on", k.toX, k.toY, ++s.stranded);
        } else {
            ++s.stranded;
        }
    }
}

}  // namespace

void beginScene() {
    State& s = state();
    const uint64_t now = GetTickCount64();
    startFades(s, now);  // 3.0.11-beta.124
    fadeFrame(s, now);
    if (s.on) strandFrame(s, now);  // 3.0.11-beta.251
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
            // 3.0.11-beta.248 (R-AMNV99 «моб бьет стоя гдето далеко»): the game moved it before the end - the
            // server's cell never told by us then
            if (s.leftTold < 20) {
                ++s.leftTold;
                LOG_INFO("net: the glide of %u to %d, %d left to the game after %u of %u ms - it moved the unit itself",
                         gl.g.id, gl.cellX, gl.cellY, unsigned(now - gl.g.startMs), gl.g.durationMs);
            }
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
            if (gameObject(actor) && gameCode(send))  // .272
                reinterpret_cast<SendMsgFn>(uintptr_t(send))(actor, 0, kPutMsg, gl.cellX, gl.cellY, 0);
            it = s.glides.erase(it);
            continue;
        }
        ++it;
    }
}

}  // namespace uagfx::win::netsmoothwin
