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
