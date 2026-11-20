// 3.0.22-beta.28 (the owner's video session 20.11: Pause's line «uaRO Graphics вимкнено…» stood in the recordings of
// the photo mode): the line in the game window's top left corner - the frame counter and the notes - and nothing of
// it in the photo mode (Ctrl+Alt+F12)
#include <string>

#include "core/overlay_line.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(overlay_line_photo_mode_shows_nothing) {
    CHECK(overlayline::line(false, true, L"uaRO Graphics вимкнено", L"60 fps") == L"");
    CHECK(overlayline::line(true, true, L"uaRO Graphics вимкнено", L"60 fps") == L"");
    CHECK(overlayline::line(true, true, L"", L"60 fps") == L"");
}

TEST(overlay_line_the_note_alone_and_with_the_counter) {
    CHECK(overlayline::line(false, false, L"uaRO Graphics вимкнено", L"60 fps") == L"uaRO Graphics вимкнено");
    CHECK(overlayline::line(true, false, L"uaRO Graphics вимкнено", L"60 fps") ==
          L"uaRO Graphics вимкнено  ·  60 fps");
}

TEST(overlay_line_the_counter_alone_or_nothing) {
    CHECK(overlayline::line(true, false, L"", L"60 fps") == L"60 fps");
    CHECK(overlayline::line(false, false, L"", L"60 fps") == L"");
}

// 3.0.22-beta.29 (the owner: «убери надпись про паузу ... пускай будет тупо старый клиент без надписи»): Pause shows
// the old client alone - no note of the driver's in its corner
TEST(overlay_notes_pause_takes_the_note_away) {
    overlayline::Notes n;
    n.say(L"Трава вимкнена", 1000, true);
    CHECK(n.shown() == L"Трава вимкнена");
    n.toggled();
    CHECK(n.shown().empty());
}

TEST(overlay_notes_under_pause_only_the_asked_one) {
    overlayline::Notes n;
    n.say(L"Відеокарта повільна", 1000, false);
    CHECK(n.shown().empty());
    n.hold(L"Тест графіки 3/9", false);
    CHECK(n.shown().empty());
    n.say(L"uaRO Graphics вимкнено", 1000, false, true);  // the graphics test's answer to its start under Pause
    CHECK(n.shown() == L"uaRO Graphics вимкнено");
}

TEST(overlay_notes_a_said_one_goes_after_10_s_a_held_one_stays) {
    overlayline::Notes n;
    n.say(L"uaRO Graphics вимкнено", 1000, false, true);
    n.tick(10999);
    CHECK(n.shown() == L"uaRO Graphics вимкнено");
    n.tick(11000);
    CHECK(n.shown().empty());
    n.say(L"Трава вимкнена", 2000, true);
    n.hold(L"Тест графіки 3/9", true);  // keep_fps's line ends no other
    n.tick(50000);
    CHECK(n.shown() == L"Тест графіки 3/9");
}
