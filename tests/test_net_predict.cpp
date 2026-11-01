// 3.0.11-beta.130 (the network plan's step 5, the own walk at once; the owner «делаем» - a switch, off at first): the
// character's walk guessed at the click, the server's answer (0x0087) joined to it (core/net_predict)
#include <cstdint>
#include <vector>

#include "core/net_predict.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// ZC_NOTIFY_PLAYERMOVE as the server packs it: its tick, then x0 y0 x1 y1 (10 bits each) and the sub-cell (4 + 4)
std::vector<uint8_t> ownMove(uint32_t tick, int x0, int y0, int x1, int y1, int sx = 8, int sy = 8) {
    std::vector<uint8_t> p(12, 0);
    p[0] = 0x87;
    for (int i = 0; i < 4; ++i) p[size_t(2 + i)] = uint8_t(tick >> (8 * i));
    p[6] = uint8_t(x0 >> 2);
    p[7] = uint8_t(((x0 & 3) << 6) | (y0 >> 4));
    p[8] = uint8_t(((y0 & 0xF) << 4) | (x1 >> 6));
    p[9] = uint8_t(((x1 & 0x3F) << 2) | (y1 >> 8));
    p[10] = uint8_t(y1 & 0xFF);
    p[11] = uint8_t((sx << 4) | sy);
    return p;
}

}  // namespace

TEST(net_predict_reads_the_servers_walk) {
    const std::vector<uint8_t> p = ownMove(0x12345678, 150, 180, 152, 175, 8, 8);
    netpredict::Move m;
    CHECK(netpredict::readOwnMove(p.data(), p.size(), m));
    CHECK(m.tick == 0x12345678 && m.srcX == 150 && m.srcY == 180 && m.dstX == 152 && m.dstY == 175);
    CHECK(m.subX == 8 && m.subY == 8);
    CHECK(!netpredict::readOwnMove(p.data(), 11, m));  // too short
    std::vector<uint8_t> q = p;
    netpredict::writeTick(q.data(), 0x01020304);
    CHECK(netpredict::readOwnMove(q.data(), q.size(), m) && m.tick == 0x01020304 && m.dstY == 175);
}

// The server walked it the guessed way (the same cells both ends): joined - its tick made the guess's, the game walks
// on from where it is; another way: the server's, as before
TEST(net_predict_joins_the_same_walk) {
    netpredict::Guess g;
    g.move.srcX = 150;
    g.move.srcY = 180;
    g.move.dstX = 152;
    g.move.dstY = 175;
    netpredict::Move server = g.move;
    server.tick = 999;
    CHECK(netpredict::answer(g, server) == netpredict::Answer::Join);
    server.srcX = 151;  // the server had it a cell on
    CHECK(netpredict::answer(g, server) == netpredict::Answer::Other);
    server = g.move;
    server.dstX = 153;  // the nearest the server could reach
    CHECK(netpredict::answer(g, server) == netpredict::Answer::Other);
}

// Which clicks are guessed (another cell, at most 20 away) and how long the server's answer is waited for (four round
// trips and 200 ms, at least 600 ms): none by then - the server did not walk it, put back
TEST(net_predict_which_walks_and_how_long) {
    CHECK(!netpredict::predicts(150, 180, 150, 180));
    CHECK(netpredict::predicts(150, 180, 153, 176));
    CHECK(netpredict::predicts(150, 180, 170, 180) && !netpredict::predicts(150, 180, 171, 180));
    CHECK(netpredict::waitMs(50) == 600 && netpredict::waitMs(300) == 1400 && netpredict::waitMs(-1) == 600);
}
