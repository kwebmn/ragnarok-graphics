// 3.0.11-beta.162 (task 8 of the remaining windows: the game's options, Alt+O): the game's options window (UI window
// 0xD) as a card (core/options_card) - the music's and the sounds' switches and volumes (0-127), the three snaps and
// the opaque windows, each the window's own control (a checkbox's 6 / 0x93 with its state, a slider's message 8 by a
// step)
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/options_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

// The window's controls (FUN_00473dd0): the checkboxes' pointers in our toggles' order (music, sounds, the attack's,
// the skills', the things' snap, opaque windows) and the two sliders'
TEST(options_card_the_windows_controls) {
    CHECK(optionscard::kToggleAt[0] == 0x7C && optionscard::kToggleAt[1] == 0x78);
    CHECK(optionscard::kToggleAt[2] == 0x80 && optionscard::kToggleAt[3] == 0x84);
    CHECK(optionscard::kToggleAt[4] == 0x88 && optionscard::kToggleAt[5] == 0x8C);
    CHECK(optionscard::kSliderAt[0] == 0x68 && optionscard::kSliderAt[1] == 0x6C);
    CHECK(optionscard::kCheckMsg == 0x93 && optionscard::kStepMsg == 8 && optionscard::kVolumeMost == 127);
}

TEST(options_card_places_and_the_mouse) {
    const optionscard::Layout L = optionscard::layout(1920, 1080, 1.0f);
    CHECK(L.x + L.w / 2 > 900 && L.x + L.w / 2 < 1020);
    CHECK(L.sliders[1].y > L.sliders[0].y && L.toggles[2].y > L.sliders[1].y && L.toggles[5].y > L.toggles[4].y);
    using P = optionscard::Part;
    const optionscard::Hit t = optionscard::hitTest(L, mid(L.toggles[3]).first, mid(L.toggles[3]).second);
    CHECK(t.part == P::Toggle && t.index == 3);
    const optionscard::Hit s = optionscard::hitTest(L, mid(L.sliders[1]).first, mid(L.sliders[1]).second);
    CHECK(s.part == P::Slider && s.index == 1);
    CHECK(optionscard::hitTest(L, mid(L.close).first, mid(L.close).second).part == P::Close);
    CHECK(optionscard::hitTest(L, -5, -5).part == P::None);
}

// 3.0.11-beta.200 (the tester: «непрозорі вікна - тогл не працює, робить непрозорі але не повертає прозорі»): uaRO.exe
// only keeps its «opaque» (m_isOpaque, the session's + 0x814) - nothing draws by it; on, the interface's windows are
// whole (100 %), off, as «Прозорість вікон» has them
TEST(options_card_opaque_windows) {
    CHECK(optionscard::windowOpacity(80, true) == 100);
    CHECK(optionscard::windowOpacity(80, false) == 80);
    CHECK(optionscard::windowOpacity(55, false) == 55);
}
