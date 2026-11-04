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

// 3.0.11-beta.134 (R-U436CU: guessed from 286, 124 while the server walked it from 290, 124 - the client still showed
// it standing, the server's walk going on): when the server's last walk of the character ends - its tick, 10 a
// straight cell and 14 a diagonal one at the character's speed (ms a cell; 150 when not told); no guess before then
TEST(net_predict_the_servers_walk_ends) {
    netpredict::Move m;
    m.tick = 1000;
    m.srcX = 0;
    m.srcY = 0;
    m.dstX = 3;
    m.dstY = 4;  // three diagonal cells and a straight one
    CHECK(netpredict::walkEnds(m, 150) == 1000 + 780);
    CHECK(netpredict::walkEnds(m, 0) == 1000 + 780);  // not told: 150
    CHECK(netpredict::walkEnds(m, 100) == 1000 + 520);
}

// 3.0.11-beta.135 (R-DWCBHA: «the server still walks the character 1797700571 ms more» with no walk known - the
// server's clock past 2^31 made 0 + 300 look ahead of it; and in a minute of lost packets the server's answers to older
// clicks came while newer guesses waited): the server's walk on only when one is known; no guess while an earlier walk
// request waits for its answer (until the answer's wait is over)
TEST(net_predict_no_guess_over_another_request) {
    CHECK(!netpredict::walking(2497266725u, 0));                     // no walk known
    CHECK(netpredict::walking(2497266725u, 2497266725u + 100));       // its walk ends later
    CHECK(!netpredict::walking(2497266725u + 400, 2497266725u + 100));  // 300 ms past its end
    CHECK(netpredict::inFlight(1000, 900, 1200, 600));    // asked at 1000, the last answer at 900: waiting
    CHECK(!netpredict::inFlight(1000, 1050, 1200, 600));  // answered
    CHECK(!netpredict::inFlight(1000, 900, 1700, 600));   // its wait over
    CHECK(!netpredict::inFlight(0, 0, 1200, 600));        // none asked
}

// 3.0.11-beta.136 (R-JMHDUY: 6 of 12 guesses walked «another way» - each time the game showed the character short of
// where the server's last walk had put it, hit by Seals on the way): guessed only when the shown cell is where the
// server last put it - its last walk's end, or its position fix when that came after the walk; none known: guessed
TEST(net_predict_guess_only_where_the_server_has_it) {
    netpredict::Move last;
    last.srcX = 97;
    last.srcY = 125;
    last.dstX = 97;
    last.dstY = 121;
    netpredict::Placed at;
    at.move = last;
    at.moveKnown = true;
    CHECK(!netpredict::caughtUp(97, 124, at));  // R-JMHDUY 04:14:33: shown 97, 124, the server had it at 97, 121
    CHECK(netpredict::caughtUp(97, 121, at));
    at.fixKnown = true;  // a fix after the walk: the server put it on 97, 123
    at.fixX = 97;
    at.fixY = 123;
    at.fixAfterMove = true;
    CHECK(netpredict::caughtUp(97, 123, at) && !netpredict::caughtUp(97, 121, at));
    netpredict::Placed none;
    CHECK(netpredict::caughtUp(228, 130, none));  // nothing known yet: the first walk after the map's entry
}

// 3.0.11-beta.137 (R-QTN6TF): guessed only on a calm line - no loss for 30 s and a round trip under 150 ms
TEST(net_predict_only_on_a_calm_line) {
    CHECK(netpredict::calm(100000, 0, 60));         // no loss seen
    CHECK(netpredict::calm(100000, 69000, 60));     // 31 s since the last
    CHECK(!netpredict::calm(100000, 75000, 60));    // 25 s
    CHECK(!netpredict::calm(100000, 0, 150));       // a slow line
    CHECK(netpredict::calm(100000, 0, 0));          // the round trip not known yet
}

// 3.0.11-beta.138 (R-VXZ36T: put back 1.3 s after the map's entry - the server walks no one before the map is loaded;
// and twice in a row in one stall - the second click guessed before TCP knew the first was lost): no guess in the
// first 5 s on a map; a guess put back counts as a lost packet (the line not calm for 30 s)
TEST(net_predict_settled_and_put_back_as_a_loss) {
    CHECK(!netpredict::settled(1000, 0));      // not on a map
    CHECK(!netpredict::settled(5000, 1000));   // 4 s on it
    CHECK(netpredict::settled(6000, 1000));    // 5 s
    CHECK(netpredict::lastLoss(9000, 12000) == 12000 && netpredict::lastLoss(15000, 12000) == 15000);
    CHECK(!netpredict::calm(20000, netpredict::lastLoss(0, 12000), 60));  // 8 s after a put back
}

// 3.0.11-beta.215 (the owner: «бывают моменты что не понимает что сидит ... приходится ещё раз отправлять команду
// сесть/встать»): the server walks no one sitting - the character's sitting from its own ZC_NOTIFY_ACT, 2 sat down and
// 3 stood up; another's, or another kind, leaves it
TEST(net_predict_knows_the_character_sits) {
    const uint32_t own = 2000001;
    netpredict::Act a;
    a.src = own;
    a.type = 2;
    CHECK(netpredict::sitsAfter(false, a, own));
    a.type = 3;
    CHECK(!netpredict::sitsAfter(true, a, own));
    a.type = 0;  // a hit of its own: whatever it was
    CHECK(netpredict::sitsAfter(true, a, own));
    CHECK(!netpredict::sitsAfter(false, a, own));
    a.src = 2000002;  // another sits down
    a.type = 2;
    CHECK(!netpredict::sitsAfter(false, a, own));
    CHECK(!netpredict::sitsAfter(false, a, 0));  // the own account not known yet
}
