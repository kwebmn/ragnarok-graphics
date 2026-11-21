// 3.0.11-beta.153 (task 3 of the remaining windows, the login's part three): the game's character make in the new look
// (core/char_make) - the stats in the game's three pairs (STR - INT, AGI - LUK, VIT - DEX, each pair 10 together) and
// the game's buttons moving a point inside a pair, the hair's style and colour, the card's places and the mouse, its
// words and picture
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/char_make.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

charmake::Texts texts() {
    charmake::Texts t;
    t.title = "Новий персонаж";
    t.name = "Ім'я";
    t.nameHint = "4–23 літери";
    t.hair = "Зачіска";
    t.colour = "Колір";
    t.pairsHint = "Очки переходять у парі: разом завжди 10";
    t.make = "Створити";
    t.cancel = "Скасувати";
    const char* const stats[6] = {"STR", "AGI", "VIT", "INT", "DEX", "LUK"};
    for (int i = 0; i < 6; ++i) t.stats[i] = stats[i];
    return t;
}

}  // namespace

// The game's pairs (FUN_0045c5f0): a point to STR 0x8b (INT gives it), to INT 0x8e, to AGI 0x8c (from LUK), to LUK
// 0x90, to VIT 0x8d (from DEX), to DEX 0x8f; the giving stat keeps 1 at least
TEST(char_make_pairs_and_the_games_buttons) {
    CHECK(charmake::kPairs[0].left == 0 && charmake::kPairs[0].right == 3);  // STR - INT
    CHECK(charmake::kPairs[1].left == 1 && charmake::kPairs[1].right == 5);  // AGI - LUK
    CHECK(charmake::kPairs[2].left == 2 && charmake::kPairs[2].right == 4);  // VIT - DEX
    CHECK(charmake::pointButton(0, true) == 0x8b && charmake::pointButton(0, false) == 0x8e);
    CHECK(charmake::pointButton(1, true) == 0x8c && charmake::pointButton(1, false) == 0x90);
    CHECK(charmake::pointButton(2, true) == 0x8d && charmake::pointButton(2, false) == 0x8f);
    charmake::Fields f;
    f.stats[0] = 9;  // STR 9, INT 1
    f.stats[3] = 1;
    CHECK(!charmake::canMove(f, 0, true) && charmake::canMove(f, 0, false));
    CHECK(charmake::canMove(f, 1, true) && charmake::canMove(f, 1, false));
}

// The hair: its style 1-23 (0xa0 on, 0xa1 back - round), its colour 0-8 (0xd5 on - round, so back is 8 times on)
TEST(char_make_hair_buttons) {
    CHECK(charmake::hairButton(true) == 0xa0 && charmake::hairButton(false) == 0xa1);
    CHECK(charmake::kColourButton == 0xd5);
    CHECK(charmake::colourPresses(true) == 1 && charmake::colourPresses(false) == 8);
    CHECK(charmake::kMakeButton == 0x76 && charmake::kCancelButton == 0x77);
}

// A card at the screen's middle: the face and the hair on the left, the name and the three pairs on the right, the two
// buttons under them; the mouse over each part
TEST(char_make_places_and_the_mouse) {
    const charmake::Layout L = charmake::layout(1920, 1080, 1.0f);
    CHECK(L.x + L.w / 2 > 900 && L.x + L.w / 2 < 1020);
    CHECK(L.y + L.h / 2 > 480 && L.y + L.h / 2 < 600);
    CHECK(L.face.x + L.face.w <= L.name.x);  // the face left of the name
    for (int i = 0; i < 3; ++i) CHECK(L.pairs[i].y > L.name.y);
    CHECK(L.make.y > L.pairs[2].y && L.cancel.y == L.make.y);
    using P = charmake::Part;
    CHECK(charmake::hitTest(L, mid(L.name).first, mid(L.name).second).part == P::Name);
    const charmake::Hit left = charmake::hitTest(L, mid(L.pairLeft[1]).first, mid(L.pairLeft[1]).second);
    CHECK(left.part == P::PairLeft && left.index == 1);
    const charmake::Hit right = charmake::hitTest(L, mid(L.pairRight[2]).first, mid(L.pairRight[2]).second);
    CHECK(right.part == P::PairRight && right.index == 2);
    CHECK(charmake::hitTest(L, mid(L.hairAfter).first, mid(L.hairAfter).second).part == P::HairAfter);
    CHECK(charmake::hitTest(L, mid(L.colourBefore).first, mid(L.colourBefore).second).part == P::ColourBefore);
    CHECK(charmake::hitTest(L, mid(L.make).first, mid(L.make).second).part == P::Make);
    CHECK(charmake::hitTest(L, mid(L.cancel).first, mid(L.cancel).second).part == P::Cancel);
    CHECK(charmake::hitTest(L, -5, -5).part == P::None);
}


// 3.0.18-beta.228 (the keyboard concept, stage 5: the character make by the keys): ↑ ↓ go round the rows - the name
// (-1: its field's caret takes ← →), the hair, its colour, the three pairs; ← → change the row chosen
TEST(char_make_rows_by_keys) {
    using charmake::keyRow;
    CHECK_EQ(keyRow(-1, true), 0);  // from the name down: the hair
    CHECK_EQ(keyRow(0, true), 1);
    CHECK_EQ(keyRow(charmake::kRows - 1, true), -1);  // past the last pair: the name again
    CHECK_EQ(keyRow(-1, false), charmake::kRows - 1);  // up from the name: the last pair
    CHECK_EQ(keyRow(0, false), -1);                    // up from the hair: the name
    CHECK_EQ(keyRow(3, false), 2);
    CHECK_EQ(keyRow(9, true), 0);  // out of the rows: as from the name
    CHECK(!charmake::keySides(-1));  // the name: ← → move its caret
    CHECK(charmake::keySides(0) && charmake::keySides(charmake::kRows - 1));
    CHECK(!charmake::keySides(charmake::kRows));
}

// 3.0.22-beta.46 (R-VE23RV «выбор статов похожим на тот как было в старом клиенте»; the owner picked variant A of
// uaro.kiev.ua/docs/char-stats: «6 угольник и остальных не надо»): the six stats on a hexagon's corners, each pair
// facing - INT over STR, AGI over LUK, DEX over VIT; a corner's press takes a point from the one facing it
TEST(char_make_hexagon_corners_face_their_pairs) {
    for (int k = 0; k < 6; ++k) {
        const int a = charmake::kHexOrder[k], b = charmake::kHexOrder[(k + 3) % 6];
        bool paired = false;
        for (const charmake::Pair& p : charmake::kPairs)
            paired = paired || (p.left == a && p.right == b) || (p.left == b && p.right == a);
        CHECK(paired);
    }
    CHECK_EQ(charmake::kHexOrder[0], 3);  // INT at the top, STR under it, as the old client
    CHECK_EQ(charmake::kHexOrder[3], 0);
    const charmake::Move str = charmake::cornerMove(0), intel = charmake::cornerMove(3);
    CHECK(str.pair == 0 && str.toLeft);
    CHECK(intel.pair == 0 && !intel.toLeft);
    CHECK(charmake::cornerMove(4).pair == 2 && !charmake::cornerMove(4).toLeft);  // DEX: VIT - DEX's right
}

TEST(char_make_hexagon_picture_follows_the_stats) {
    charmake::Fields f;
    const panel::Picture even = charmake::hexPicture(f.stats, 200);
    CHECK_EQ(even.width, 200);
    CHECK_EQ(even.height, 200);
    if (even.width != 200) return;
    const auto alpha = [](const panel::Picture& p, int x, int y) {
        return p.px[size_t(y) * size_t(p.width) + size_t(x)] >> 24;
    };
    // up from the middle: inside the shape at a third of the way with 5 points; at 85 % only with INT 9
    const int c = 100, r = charmake::hexRadius(200);
    CHECK(alpha(even, c, c - r / 3) > alpha(even, c + 1, 3));
    f.stats[3] = 9, f.stats[0] = 1;
    const panel::Picture tall = charmake::hexPicture(f.stats, 200);
    const int y = c - r * 85 / 100;
    const auto red = [](const panel::Picture& p, int x, int y) {
        return (p.px[size_t(y) * size_t(p.width) + size_t(x)] >> 16) & 255;
    };
    CHECK(red(tall, c, y) > red(even, c, y) + 40);  // gold there now, the dark ground before
}
