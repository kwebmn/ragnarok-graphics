// 3.0.3-beta.2: each character's game window at its own place - uaro-graphics\uaro-windows.txt (core/window_places)
#include <string>

#include "core/window_places.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(window_places_read_a_line_each) {
    const places::Places p = places::parse(
        "; uaRO Graphics: where each character's game window was put\n"
        "120 80 Lena\r\n"
        "-1700 40 Big Bob\n"
        "\n"
        "12 x Name\n"  // not a number: skipped
        "5 6\n"        // no name: skipped
        "7 8 \n"       // an empty name: skipped
        "300 200 Lena\n");  // the last one counts
    CHECK(p.size() == 2);
    CHECK(p.at("Lena").x == 300 && p.at("Lena").y == 200);
    CHECK(p.at("Big Bob").x == -1700 && p.at("Big Bob").y == 40);  // left of the main monitor
}

TEST(window_places_written_and_read_back) {
    places::Places p;
    p["Lena"] = places::Place{120, 80};
    p["\xcf\xf0\xe8\xf1\xf2 2"] = places::Place{-5, 1100};
    const std::string text = places::format(p);
    CHECK(text.rfind(";", 0) == 0);  // it says what it is first
    const places::Places back = places::parse(text);
    CHECK(back.size() == 2);
    CHECK(back.at("Lena").x == 120 && back.at("Lena").y == 80);
    CHECK(back.at("\xcf\xf0\xe8\xf1\xf2 2").x == -5 && back.at("\xcf\xf0\xe8\xf1\xf2 2").y == 1100);
    CHECK(places::parse("").empty());
}

namespace {

using places::Place;
using places::Step;

// The places file as the tracker asks it: one character's place, or none
struct File {
    std::string name;
    Place place;
    int asked = 0;
    places::Lookup lookup() {
        return [this](const std::string& who) -> std::optional<Place> {
            ++asked;
            if (who == name) return place;
            return std::nullopt;
        };
    }
};

bool is(const Step& s, Step::Do what, int x = 0, int y = 0) {
    return s.what == what && (what == Step::Do::Nothing || (s.place.x == x && s.place.y == y));
}

}  // namespace

TEST(window_place_tracker_moves_a_coming_character_to_its_place) {
    File f{"Lena", Place{100, 50}};
    places::Tracker t;
    CHECK(is(t.step("", true, Place{0, 0}, 0, f.lookup()), Step::Do::Nothing));  // the login: nobody yet
    CHECK(f.asked == 0);
    CHECK(is(t.step("Lena", true, Place{0, 0}, 10, f.lookup()), Step::Do::Move, 100, 50));
    t.at(Place{100, 50});
    CHECK(is(t.step("Lena", true, Place{100, 50}, 20, f.lookup()), Step::Do::Nothing));
    CHECK(f.asked == 1);  // the file asked once, as the character came
    places::Tracker same;  // already there: no move
    CHECK(is(same.step("Lena", true, Place{100, 50}, 0, f.lookup()), Step::Do::Nothing));
    places::Tracker other;  // a character with no place yet stays where it is
    CHECK(is(other.step("Bob", true, Place{7, 7}, 0, f.lookup()), Step::Do::Nothing));
}

TEST(window_place_tracker_keeps_a_window_once_it_stays_a_second) {
    File f;
    places::Tracker t;
    CHECK(is(t.step("Lena", true, Place{10, 10}, 0, f.lookup()), Step::Do::Nothing));
    CHECK(is(t.step("Lena", true, Place{300, 200}, 100, f.lookup()), Step::Do::Nothing));  // just moved
    CHECK(is(t.step("Lena", true, Place{300, 200}, 600, f.lookup()), Step::Do::Nothing));
    CHECK(is(t.step("Lena", true, Place{300, 200}, 1100, f.lookup()), Step::Do::Keep, 300, 200));
    CHECK(is(t.step("Lena", true, Place{300, 200}, 1200, f.lookup()), Step::Do::Nothing));  // kept once
    CHECK(is(t.step("Lena", true, Place{400, 200}, 1300, f.lookup()), Step::Do::Nothing));  // moved on
    CHECK(is(t.step("Lena", true, Place{420, 210}, 2000, f.lookup()), Step::Do::Nothing));  // and on: the wait anew
    CHECK(is(t.step("Lena", true, Place{420, 210}, 2900, f.lookup()), Step::Do::Nothing));
    CHECK(is(t.step("Lena", true, Place{420, 210}, 3000, f.lookup()), Step::Do::Keep, 420, 210));
}

TEST(window_place_tracker_leaves_a_window_the_player_does_not_move) {
    File f{"Lena", Place{100, 50}};
    places::Tracker t;
    // locked or maximised: never moved, nothing kept
    CHECK(is(t.step("Lena", false, Place{0, 0}, 0, f.lookup()), Step::Do::Nothing));
    CHECK(is(t.step("Lena", false, Place{0, 0}, 5000, f.lookup()), Step::Do::Nothing));
    // back to a normal window: where it is now is no move of the player's
    CHECK(is(t.step("Lena", true, Place{30, 30}, 6000, f.lookup()), Step::Do::Nothing));
    CHECK(is(t.step("Lena", true, Place{30, 30}, 8000, f.lookup()), Step::Do::Nothing));
    // another character in the same window: its own place
    File g{"Bob", Place{900, 10}};
    CHECK(is(t.step("Bob", true, Place{30, 30}, 9000, g.lookup()), Step::Do::Move, 900, 10));
}
