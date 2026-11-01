#include "win/net_predict.hpp"

#include <windows.h>

#include <cmath>
#include <cstdint>

#include "core/char_status.hpp"
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
    uint32_t holdUntil = 0;  // 3.0.11-beta.132: the server's clock the own walk is held to (hit, attacking)
    int heldTold = 0;
    // 3.0.11-beta.134: the server's last walk of the character (when it ends at its speed), its last position fix
    np::Move lastMove;
    uint64_t lastMoveAt = 0, fixAt = 0;
    uint32_t walkEnd = 0;
    int speed = 0, fixX = 0, fixY = 0, walkingTold = 0;
    // 3.0.11-beta.135 (R-DWCBHA): the last two walk requests' times (the one before the guessed one still waiting:
    // no guess), the bad connection told
    uint64_t askedAt = 0, prevAskedAt = 0;
    int flightTold = 0, badTold = 0;
    np::Placed placed;  // 3.0.11-beta.136: where the server last put the character (its walk's end or a fix)
    int behindTold = 0;
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

bool dead() {
    charstatus::Status st;
    return readOwnStatus(st) && st.hpMost > 0 && st.hp <= 0;
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
    if (!still || casting(actor) || !np::predicts(x, y, s.askX, s.askY) || dead()) return;
    if (const uint32_t clock = netwatchwin::serverNow(); np::heldNow(clock, s.holdUntil)) {  // 3.0.11-beta.132
        if (s.heldTold++ < 10)
            LOG_INFO("net: the walk not guessed - the server holds the character %d ms more (hit or attacking)",
                     int(int32_t(s.holdUntil - clock)));
        return;
    } else if (np::walking(clock, s.walkEnd)) {  // 3.0.11-beta.134 (.135: only a walk known)
        if (s.walkingTold++ < 10)
            LOG_INFO("net: the walk not guessed - the server still walks the character %d ms more (shown at %d, %d; "
                     "its walk %d, %d to %d, %d)", int(int32_t(s.walkEnd - clock)), x, y, s.lastMove.srcX,
                     s.lastMove.srcY, s.lastMove.dstX, s.lastMove.dstY);
        return;
    }
    // 3.0.11-beta.135: packets lost now - the answers late and out of turn: no guess; 3.0.11-beta.137 (R-QTN6TF: both
    // put back were guessed in a calm moment of a lossy line): none lost for 30 s, a round trip under 150 ms
    if (!np::calm(now, netwatchwin::lastLossMs(), netwatchwin::rttMs())) {
        if (s.badTold++ < 10)
            LOG_INFO("net: the walk not guessed - the line is not calm (a packet lost %llu s ago, round trip %d ms)",
                     static_cast<unsigned long long>(netwatchwin::lastLossMs()
                                                         ? (now - netwatchwin::lastLossMs()) / 1000 : 0),
                     netwatchwin::rttMs());
        return;
    }
    if (!np::caughtUp(x, y, s.placed)) {  // 3.0.11-beta.136: the game shows it short of the server's cell
        if (s.behindTold++ < 10)
            LOG_INFO("net: the walk not guessed - shown at %d, %d, the server put it on %d, %d", x, y,
                     s.placed.fixAfterMove ? s.placed.fixX : s.placed.move.dstX,
                     s.placed.fixAfterMove ? s.placed.fixY : s.placed.move.dstY);
        return;
    }
    if (np::inFlight(s.prevAskedAt, s.lastMoveAt, now, np::waitMs(netwatchwin::rttMs()))) {  // 3.0.11-beta.135
        if (s.flightTold++ < 10)
            LOG_INFO("net: the walk not guessed - the walk asked before still waits for the server");
        return;
    }
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
    s.prevAskedAt = s.askedAt;  // 3.0.11-beta.135
    s.askedAt = GetTickCount64();
    s.asked = true;
    s.askX = x;
    s.askY = y;
}

void ownMove(uint8_t* packet) {
    State& s = state();
    s.asked = false;  // the server's answer before the frame guessed it: the game walks by it alone
    np::Move m;
    if (!s.on || !np::readOwnMove(packet, 12, m)) return;
    const uint64_t now = GetTickCount64();
    const np::Move before = s.lastMove;
    const uint64_t beforeAt = s.lastMoveAt;
    s.lastMove = m;  // 3.0.11-beta.134: when this walk ends on the server
    s.lastMoveAt = now;
    s.walkEnd = np::walkEnds(m, s.speed);
    s.placed.move = m;  // 3.0.11-beta.136: the server's cell is its end now
    s.placed.moveKnown = true;
    s.placed.fixAfterMove = false;
    if (!s.guess.pending) return;
    s.guess.pending = false;
    const uint64_t late = now - s.guess.atMs;
    if (np::answer(s.guess, m) == np::Answer::Join) {
        np::writeTick(packet, s.guess.move.tick);  // the game walks on from where the guess has it
        s.lastMove.tick = s.guess.move.tick;
        s.walkEnd = np::walkEnds(s.lastMove, s.speed);
        if (++s.joined <= 5 || s.joined % 50 == 0)
            LOG_INFO("net: the server's walk came %llu ms after the guess - the same way, joined (%d so far)",
                     static_cast<unsigned long long>(late), s.joined);
        return;
    }
    if (++s.other <= 20)  // 3.0.11-beta.134: with the server's walk before it and the last fix, to see why
        LOG_INFO("net: the server walked another way (from %d, %d to %d, %d; guessed %d, %d to %d, %d) - its own; "
                 "its walk before %d, %d to %d, %d %llu ms ago, the last fix %d, %d %llu ms ago", m.srcX, m.srcY,
                 m.dstX, m.dstY, s.guess.move.srcX, s.guess.move.srcY, s.guess.move.dstX, s.guess.move.dstY,
                 before.srcX, before.srcY, before.dstX, before.dstY,
                 static_cast<unsigned long long>(beforeAt ? now - beforeAt : 0), s.fixX, s.fixY,
                 static_cast<unsigned long long>(s.fixAt ? now - s.fixAt : 0));
}

void speed(int msPerCell) {
    State& s = state();
    if (msPerCell > 0 && msPerCell < 2000) s.speed = msPerCell;
}

void ownFix(int x, int y) {
    State& s = state();
    s.fixX = x;
    s.fixY = y;
    s.fixAt = GetTickCount64();
    s.placed.fixX = x;  // 3.0.11-beta.136: the server put it there after its last walk
    s.placed.fixY = y;
    s.placed.fixKnown = true;
    s.placed.fixAfterMove = true;
}

void act(const uint8_t* packet) {
    State& s = state();
    np::Act a;
    if (s.on && np::readAct(packet, 29, a)) s.holdUntil = np::holdUntil(s.holdUntil, a, read32(kOwnAccount));
}

void beginScene() {
    State& s = state();
    if (!s.on) return;
    const uint64_t now = GetTickCount64();
    float at[3] = {};
    int w = 0, h = 0, cell = 0;
    if (!inGameWorld() || !readPlayerWorld(at, w, h, cell)) {
        s.placed = np::Placed{};  // 3.0.11-beta.136: another map - nothing known there yet
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
        if (dead()) return;  // 3.0.11-beta.132: died meanwhile - the game shows it as the server has it
        tell(player(), kPutMsg, s.guess.move.srcX, s.guess.move.srcY);
        if (++s.putBack <= 20)
            LOG_INFO("net: no answer to the guessed walk in %u ms - the server did not walk it: put back on %d, %d",
                     np::waitMs(netwatchwin::rttMs()), s.guess.move.srcX, s.guess.move.srcY);
    }
    if (s.asked) guess(s, now);
}

}  // namespace uagfx::win::netpredictwin
