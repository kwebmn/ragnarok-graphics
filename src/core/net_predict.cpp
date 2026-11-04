#include "core/net_predict.hpp"

#include <algorithm>
#include <cstdlib>

namespace uagfx::netpredict {

bool readOwnMove(const uint8_t* p, size_t size, Move& m) {
    if (!p || size < 12) return false;
    m.tick = uint32_t(p[2]) | uint32_t(p[3]) << 8 | uint32_t(p[4]) << 16 | uint32_t(p[5]) << 24;
    m.srcX = int(p[6]) << 2 | int(p[7]) >> 6;
    m.srcY = int(p[7] & 0x3F) << 4 | int(p[8]) >> 4;
    m.dstX = int(p[8] & 0x0F) << 6 | int(p[9]) >> 2;
    m.dstY = int(p[9] & 0x03) << 8 | int(p[10]);
    m.subX = int(p[11]) >> 4;
    m.subY = int(p[11] & 0x0F);
    return true;
}

void writeTick(uint8_t* p, uint32_t tick) {
    for (int i = 0; i < 4; ++i) p[2 + i] = uint8_t(tick >> (8 * i));
}

Answer answer(const Guess& g, const Move& s) {
    const Move& m = g.move;
    return m.srcX == s.srcX && m.srcY == s.srcY && m.dstX == s.dstX && m.dstY == s.dstY ? Answer::Join
                                                                                        : Answer::Other;
}

bool predicts(int fromX, int fromY, int toX, int toY) {
    const int far = std::max(std::abs(toX - fromX), std::abs(toY - fromY));
    return far > 0 && far <= 20;
}

uint32_t waitMs(int rttMs) { return uint32_t(std::max(600, 4 * std::max(0, rttMs) + 200)); }

namespace {

uint32_t rd32(const uint8_t* p, size_t at) {
    return uint32_t(p[at]) | uint32_t(p[at + 1]) << 8 | uint32_t(p[at + 2]) << 16 | uint32_t(p[at + 3]) << 24;
}

// The later of two of the server's ticks (they wrap)
uint32_t later(uint32_t a, uint32_t b) { return int32_t(b - a) > 0 ? b : a; }

bool attackKind(int type) { return type == 0 || type == 8 || type == 10; }  // a hit, hits, a critical

}  // namespace

bool readAct(const uint8_t* p, size_t size, Act& a) {
    if (!p || size < 29) return false;
    a.src = rd32(p, 2);
    a.dst = rd32(p, 6);
    a.tick = rd32(p, 10);
    a.srcSpeed = int(rd32(p, 14));
    a.dstSpeed = int(rd32(p, 18));
    a.damage = int(int16_t(uint16_t(p[22] | p[23] << 8)));
    a.type = int(p[26]);
    return true;
}

uint32_t holdUntil(uint32_t until, const Act& a, uint32_t own) {
    if (!own) return until;
    if (a.dst == own && a.damage > 0 && attackKind(a.type) && a.dstSpeed > 0)  // hit: its damage motion
        return until ? later(until, a.tick + uint32_t(a.dstSpeed)) : a.tick + uint32_t(a.dstSpeed);
    if (a.src == own && (attackKind(a.type) || a.type == 4 || a.type == 9) && a.srcSpeed > 0)  // it attacked
        return until ? later(until, a.tick + uint32_t(a.srcSpeed)) : a.tick + uint32_t(a.srcSpeed);
    return until;
}

bool heldNow(uint32_t serverNow, uint32_t until) { return until && int32_t(serverNow - (until + 50)) < 0; }

bool sitsAfter(bool sitting, const Act& a, uint32_t own) {
    if (!own || a.src != own || (a.type != 2 && a.type != 3)) return sitting;
    return a.type == 2;
}

bool walking(uint32_t serverNow, uint32_t walkEnd) { return walkEnd && heldNow(serverNow, walkEnd + 250); }

bool inFlight(uint64_t askedMs, uint64_t answeredMs, uint64_t nowMs, uint32_t waitMs) {
    return askedMs && askedMs > answeredMs && nowMs >= askedMs && nowMs - askedMs < waitMs;
}

bool caughtUp(int shownX, int shownY, const Placed& at) {
    if (at.fixKnown && (at.fixAfterMove || !at.moveKnown)) return shownX == at.fixX && shownY == at.fixY;
    if (at.moveKnown) return shownX == at.move.dstX && shownY == at.move.dstY;
    return true;
}

bool calm(uint64_t nowMs, uint64_t lastLossMs, int rttMs) {
    return (!lastLossMs || nowMs < lastLossMs || nowMs - lastLossMs >= 30000) && rttMs < 150;
}

bool settled(uint64_t nowMs, uint64_t enteredMs) {
    return enteredMs && nowMs >= enteredMs && nowMs - enteredMs >= 5000;
}

uint32_t walkEnds(const Move& m, int speedMs) {
    const int dx = std::abs(m.dstX - m.srcX), dy = std::abs(m.dstY - m.srcY);
    const int diagonal = std::min(dx, dy), straight = std::max(dx, dy) - diagonal;
    const int speed = speedMs > 0 ? speedMs : 150;
    return m.tick + uint32_t(speed * (14 * diagonal + 10 * straight) / 10);
}

}  // namespace uagfx::netpredict
