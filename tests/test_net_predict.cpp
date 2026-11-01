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

// 3.0.11-beta.132 (R-6D43Y6: a walk guessed while five Seals hit the character - the server holds a hit or attacking
// unit still for its damage or attack motion, so no answer came and it was put back): the server's hold on the own
// walk from its acts (0x008a) - hit (damage, not endured, not missed): its damage motion; attacking: its attack motion
TEST(net_predict_the_servers_hold_on_the_walk) {
    netpredict::Act hit;
    hit.src = 110021272;
    hit.dst = 2000000;
    hit.tick = 1000;
    hit.srcSpeed = 872;
    hit.dstSpeed = 480;
    hit.damage = 120;
    hit.type = 0;
    CHECK(netpredict::holdUntil(0, hit, 2000000) == 1480);
    CHECK(netpredict::holdUntil(1600, hit, 2000000) == 1600);  // a longer hold kept
    netpredict::Act endured = hit;
    endured.type = 4;
    CHECK(netpredict::holdUntil(0, endured, 2000000) == 0);
    netpredict::Act missed = hit;
    missed.damage = 0;
    CHECK(netpredict::holdUntil(0, missed, 2000000) == 0);
    netpredict::Act attack = hit;  // the character hits a Seal
    attack.src = 2000000;
    attack.dst = 110021272;
    attack.tick = 3000;
    attack.srcSpeed = 384;
    CHECK(netpredict::holdUntil(0, attack, 2000000) == 3384);
    netpredict::Act others = hit;  // another's fight
    others.dst = 2000001;
    CHECK(netpredict::holdUntil(0, others, 2000000) == 0);
    CHECK(netpredict::heldNow(1500, 1480) && !netpredict::heldNow(1531, 1480));  // 50 ms over it
}

// ZC_NOTIFY_ACT (29 bytes): who +2, whom +6, the server's tick +10, the attack's motion +14, the damage's motion +18,
// the damage +22, the hits +24, the kind +26
TEST(net_predict_reads_an_act) {
    std::vector<uint8_t> p(29, 0);
    p[0] = 0x8A;
    const auto put32 = [&](size_t at, uint32_t v) {
        for (int i = 0; i < 4; ++i) p[at + size_t(i)] = uint8_t(v >> (8 * i));
    };
    put32(2, 110021272);
    put32(6, 2000000);
    put32(10, 123456);
    put32(14, 872);
    put32(18, 480);
    p[22] = 120;
    p[26] = 10;  // a critical hit
    netpredict::Act a;
    CHECK(netpredict::readAct(p.data(), p.size(), a));
    CHECK(a.src == 110021272 && a.dst == 2000000 && a.tick == 123456 && a.srcSpeed == 872 && a.dstSpeed == 480);
    CHECK(a.damage == 120 && a.type == 10);
    CHECK(!netpredict::readAct(p.data(), 28, a));
}
