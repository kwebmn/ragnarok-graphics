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

}  // namespace uagfx::netpredict
