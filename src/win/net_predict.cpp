#include "win/net_predict.hpp"

#include <windows.h>

#include <cmath>
#include <cstdint>

#include "core/net_predict.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/net_watch.hpp"

namespace uagfx::win::netpredictwin {
namespace {

namespace np = uagfx::netpredict;

// uaRO.exe's: the character's own account; a unit's message (its vtable + 8, thiscall: sender, message, a, b, c) - 0
// a walk (a: FUN_0057e1f0's struct), 0x13 put on the cell (a, b); the player's cast gauge (+0x220, its class) while it
// casts (win/cast_bar's)
constexpr uint32_t kOwnAccount = 0x771F7C, kSendMsgSlot = 8, kGauge = 0x220, kGaugeClass = 0x696C18;
constexpr int kWalkMsg = 0, kPutMsg = 0x13;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
constexpr uint64_t kStillMs = 150;  // standing so long - not walking, not stepping

struct State {
    bool on = false;
    bool asked = false;  // a walk request seen, guessed at the next frame
    int askX = 0, askY = 0;
    np::Guess guess;
    float lastX = 0, lastZ = 0;
    uint64_t stillSince = 0;
    int joined = 0, other = 0, putBack = 0, guessed = 0, told = 0;
};

State& state() {
    static State s;
    return s;
}

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

uint32_t player() {
    const uint32_t own = read32(kOwnAccount);
    return own ? actorAddress(own) : 0;
}

bool tell(uint32_t actor, int msg, int a, int b) {
    const uint32_t send = actor ? read32(read32(actor) + kSendMsgSlot) : 0;
    if (!send) return false;
    reinterpret_cast<SendMsgFn>(uintptr_t(send))(actor, 0, msg, a, b, 0);
    return true;
}

bool casting(uint32_t actor) {
    const uint32_t gauge = read32(actor + kGauge);
    return gauge && read32(gauge) == kGaugeClass;
}

void guess(State& s, uint64_t now) {
    s.asked = false;
    if (s.guess.pending) return;  // the last one not answered yet
    int x = 0, y = 0, w = 0, h = 0;
    const uint32_t actor = player();
    if (!actor || !readPlayerCell(x, y, w, h)) return;
    const bool still = s.stillSince && now - s.stillSince >= kStillMs;
    if (!still || casting(actor) || !np::predicts(x, y, s.askX, s.askY)) return;
    np::Move m;
    m.tick = netwatchwin::serverNow();
    m.srcX = x;
    m.srcY = y;
    m.dstX = s.askX;
    m.dstY = s.askY;
    uint32_t told[7] = {m.tick, uint32_t(m.srcX), uint32_t(m.srcY), uint32_t(m.subX), uint32_t(m.subY),
                        uint32_t(m.dstX), uint32_t(m.dstY)};
    if (!tell(actor, kWalkMsg, int(uintptr_t(told)), 0)) return;
    s.guess.pending = true;
    s.guess.move = m;
    s.guess.atMs = now;
    if (++s.guessed <= 5)
        LOG_INFO("net: the walk guessed - from %d, %d to %d, %d at the click (the server's answer joined when it "
                 "comes)", x, y, m.dstX, m.dstY);
}

}  // namespace

void setOn(bool on) {
    State& s = state();
    if (on == s.on) return;
    s.on = on;
    s.asked = false;
    s.guess.pending = false;
    LOG_INFO("net: [interface] predict_walk = %d", on ? 1 : 0);
}

void walkAsked(int x, int y) {
    State& s = state();
    if (!s.on) return;
    s.asked = true;
    s.askX = x;
    s.askY = y;
}

void ownMove(uint8_t* packet) {
    State& s = state();
    s.asked = false;  // the server's answer before the frame guessed it: the game walks by it alone
    np::Move m;
    if (!s.guess.pending || !np::readOwnMove(packet, 12, m)) return;
    s.guess.pending = false;
    const uint64_t late = GetTickCount64() - s.guess.atMs;
    if (np::answer(s.guess, m) == np::Answer::Join) {
        np::writeTick(packet, s.guess.move.tick);  // the game walks on from where the guess has it
        if (++s.joined <= 5 || s.joined % 50 == 0)
            LOG_INFO("net: the server's walk came %llu ms after the guess - the same way, joined (%d so far)",
                     static_cast<unsigned long long>(late), s.joined);
        return;
    }
    if (++s.other <= 20)
        LOG_INFO("net: the server walked another way (from %d, %d to %d, %d; guessed %d, %d to %d, %d) - its own",
                 m.srcX, m.srcY, m.dstX, m.dstY, s.guess.move.srcX, s.guess.move.srcY, s.guess.move.dstX,
                 s.guess.move.dstY);
}

void beginScene() {
    State& s = state();
    if (!s.on) return;
    const uint64_t now = GetTickCount64();
    float at[3] = {};
    int w = 0, h = 0, cell = 0;
    if (!inGameWorld() || !readPlayerWorld(at, w, h, cell)) {
        s.stillSince = 0;
        s.asked = false;
        s.guess.pending = false;
        return;
    }
    if (std::fabs(at[0] - s.lastX) + std::fabs(at[2] - s.lastZ) > 0.01f || !s.stillSince) s.stillSince = now;
    s.lastX = at[0];
    s.lastZ = at[2];
    if (s.guess.pending && now - s.guess.atMs > np::waitMs(netwatchwin::rttMs())) {  // the server did not walk it
        s.guess.pending = false;
        tell(player(), kPutMsg, s.guess.move.srcX, s.guess.move.srcY);
        if (++s.putBack <= 20)
            LOG_INFO("net: no answer to the guessed walk in %u ms - the server did not walk it: put back on %d, %d",
                     np::waitMs(netwatchwin::rttMs()), s.guess.move.srcX, s.guess.move.srcY);
    }
    if (s.asked) guess(s, now);
}

}  // namespace uagfx::win::netpredictwin
