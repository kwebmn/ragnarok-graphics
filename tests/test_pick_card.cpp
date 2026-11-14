// 3.0.11-beta.157 (task 7 of the remaining windows: identify and the card's compound): the game's «choose a thing»
// windows (the magnifier's list - UI window 0x49, a card's list of things it goes into - 0x4A) as one card
// (core/pick_card): a row a thing, the chosen one outlined, OK and cancel the game's own (0x76 / 0x77)
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/pick_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

TEST(pick_card_buttons_are_the_games) {
    CHECK(pickcard::kOkButton == 0x76 && pickcard::kCancelButton == 0x77);
}

// At the middle; a row a thing (7 shown at most, the wheel for the rest), OK and cancel under them; the mouse on each
TEST(pick_card_places_and_the_mouse) {
    const pickcard::Layout L = pickcard::layout(1920, 1080, 1.0f, 3);
    CHECK(L.rows.size() == 3);
    CHECK(L.x + L.w / 2 > 900 && L.x + L.w / 2 < 1020);
    CHECK(L.ok.y > L.rows[2].y && L.cancel.y == L.ok.y);
    using P = pickcard::Part;
    const pickcard::Hit row = pickcard::hitTest(L, 0, 3, mid(L.rows[1]).first, mid(L.rows[1]).second);
    CHECK(row.part == P::Row && row.row == 1);
    CHECK(pickcard::hitTest(L, 0, 3, mid(L.ok).first, mid(L.ok).second).part == P::Ok);
    CHECK(pickcard::hitTest(L, 0, 3, mid(L.cancel).first, mid(L.cancel).second).part == P::Cancel);
    CHECK(pickcard::hitTest(L, 0, 3, -5, -5).part == P::None);
    const pickcard::Layout many = pickcard::layout(1920, 1080, 1.0f, 12);
    CHECK(many.rows.size() == 7 && pickcard::mostScroll(many, 12) == 5);
    const pickcard::Hit scrolled = pickcard::hitTest(many, 4, 12, mid(many.rows[0]).first, mid(many.rows[0]).second);
    CHECK(scrolled.part == P::Row && scrolled.row == 4);
    CHECK(pickcard::layout(1920, 1080, 1.0f, 0).rows.empty());  // nothing to choose: the empty line instead
}

// 3.0.11-beta.171 (the login concept: «Выбор сервера: пропускаем»): the server select with one row is chosen and
// confirmed at once - the game shows it even for one (uaRO HD's sclientinfo has one connection), after the login
// the char servers' list too; two rows and more wait for the player
TEST(pick_card_single_entry_goes_on_by_itself) {
    CHECK(pickcard::autoChoice(1, -1) == 0);
    CHECK(pickcard::autoChoice(1, 0) == 0);
    CHECK(pickcard::autoChoice(2, 0) == -1);
    CHECK(pickcard::autoChoice(0, -1) == -1);
}


// 3.0.19-beta.1 (the testers' doc 14.11: «тут код карти прописує, а встарому назви і це удобніше»): a Warp Portal /
// Teleport row naming a map - the client's name of it (data\mapnametable.txt, whole, a space after its commas), its
// code under it; a row not a map's (Random) as it is
TEST(pick_card_a_warp_row_names_its_map) {
    const std::map<std::string, std::string> names = {{"comodo", "Beach town,Comodo"},
                                                      {"kh_dun02", "Robot Factory level 2"},
                                                      {"jupe_ele_r", "Juperos Elevator"},
                                                      {"ra_san04", "Holy Ground"}};
    pickcard::Place p = pickcard::placeOf(names, "kh_dun02");
    CHECK_EQ(p.name, std::string("Robot Factory level 2"));
    CHECK_EQ(p.code, std::string("kh_dun02"));
    p = pickcard::placeOf(names, "comodo.gat");
    CHECK_EQ(p.name, std::string("Beach town, Comodo"));
    CHECK_EQ(p.code, std::string("comodo"));
    p = pickcard::placeOf(names, "Comodo");
    CHECK_EQ(p.name, std::string("Beach town, Comodo"));
    p = pickcard::placeOf(names, "Random");
    CHECK_EQ(p.name, std::string("Random"));
    CHECK_EQ(p.code, std::string());
    p = pickcard::placeOf({}, "jupe_ele_r");  // no table read: the code as it is
    CHECK_EQ(p.name, std::string("jupe_ele_r"));
    CHECK_EQ(p.code, std::string());
}
