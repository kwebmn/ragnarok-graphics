// 3.0.11-beta.203 (the tester: «при логіні, коли скріпт швиденько проклікує вікна з персонажом ... я б
// взагалі спробував позбутися проклікування, або приховав це додатковим вікном завантаження»): while the login
// enters the game by itself (one click: the login, the character select, on to the map) the screen dark under a
// card - «Вхід у гру…», the character - till the map starts, a box asks (a refusal, an error), the login card is
// back, or 30 s
#include <algorithm>
#include <string>

#include "core/enter_veil.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(enter_veil_card_in_the_middle) {
    const enterveil::Layout L = enterveil::layout(1920, 1080, 1.0f);
    CHECK(L.card.w > 200 && L.card.h > 60);
    CHECK(std::abs(L.x + L.card.x + L.card.w / 2 - 960) <= 2 && std::abs(L.y + L.card.y + L.card.h / 2 - 540) <= 40);
    enterveil::Texts t;
    t.title = "Вхід у гру…";
    t.name = "Людмила Павличенко";
    t.line = "Gunslinger · 64";
    const std::vector<enterveil::Words> w = enterveil::words(L, t);
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const enterveil::Words& x) { return x.text == s; });
    };
    CHECK(has("Вхід у гру…") && has("Людмила Павличенко") && has("Gunslinger · 64"));
}

TEST(enter_veil_ends) {
    using E = enterveil::End;
    CHECK(enterveil::ending(false, false, false, 1000, 5000) == E::No);
    CHECK(enterveil::ending(true, false, false, 1000, 5000) == E::World);  // the map's start: its own loading picture
    CHECK(enterveil::ending(false, true, false, 1000, 5000) == E::Box);    // a box asks: shown, not hidden
    CHECK(enterveil::ending(false, false, true, 1000, 5000) == E::Login);  // the login card back (not entered)
    CHECK(enterveil::ending(false, false, false, 1000, 31001) == E::Late);
}

// 3.0.18-beta.164 (R-CFNPF6, the tester: «не пускает в игру ... будет такая загрузка вечно»): the map entry's pause
// held the character's selection 160 s while the veil went at 30 s and a small line at the screen's top alone told
// why - while the pause holds, the veil stays past its 30 s (a box or the login card back still end it), its line the
// time left
TEST(enter_veil_stays_while_the_map_entry_pause_holds) {
    using E = enterveil::End;
    CHECK(enterveil::ending(false, false, false, 1000, 200000, true) == E::No);  // no 30 s while held
    CHECK(enterveil::ending(false, true, false, 1000, 200000, true) == E::Box);  // a box asks: shown all the same
    CHECK(enterveil::ending(false, false, true, 1000, 5000, true) == E::Login);
    CHECK(enterveil::ending(true, false, false, 1000, 5000, true) == E::World);
    CHECK(enterveil::ending(false, false, false, 1000, 31001, false) == E::Late);  // let go: its 30 s again
    CHECK(enterveil::clock(147) == "2:27");
    CHECK(enterveil::clock(5) == "0:05");
    CHECK(enterveil::clock(0) == "0:00");
}
