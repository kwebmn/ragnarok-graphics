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

// A slider's place to its volume: its left end 0, its right end 127, in steps between, outside it clamped
TEST(options_card_slider_to_volume) {
    const optionscard::Layout L = optionscard::layout(1920, 1080, 1.0f);
    const panel::Box& b = L.sliders[0];
    CHECK(optionscard::volumeAt(b, b.x) == 0 && optionscard::volumeAt(b, b.x + b.w) == 127);
    CHECK(optionscard::volumeAt(b, b.x - 50) == 0 && optionscard::volumeAt(b, b.x + b.w + 50) == 127);
    const int half = optionscard::volumeAt(b, b.x + b.w / 2);
    CHECK(half >= 62 && half <= 65);
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

TEST(options_card_words_and_picture) {
    const optionscard::Layout L = optionscard::layout(1920, 1080, 1.0f);
    optionscard::Values v;
    v.on[0] = true;
    v.volume[0] = 64;
    v.volume[1] = 127;
    optionscard::Texts t;
    t.title = "Налаштування гри";
    const char* const names[6] = {"Музика", "Звуки", "Прилипання: атака", "Прилипання: навички", "Прилипання: речі",
                                  "Непрозорі вікна"};
    for (int i = 0; i < 6; ++i) t.toggles[i] = names[i];
    t.snapHint = "Курсор сам наводиться на найближчу ціль";
    const std::vector<optionscard::Words> w = optionscard::words(L, v, optionscard::Show{}, t);
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const optionscard::Words& x) { return x.text == s; });
    };
    CHECK(has("Налаштування гри") && has("Музика") && has("Прилипання: речі") && has("Непрозорі вікна"));
    CHECK(has("50%") && has("100%"));  // the volumes as shares
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    optionscard::paint(p, L, v, optionscard::Show{});
    CHECK((p.px[size_t(L.h / 2) * size_t(L.w) + size_t(L.w / 2)] >> 24) > 0x80);
}

// 3.0.11-beta.200 (the tester: «непрозорі вікна - тогл не працює, робить непрозорі але не повертає прозорі»): uaRO.exe
// only keeps its «opaque» (m_isOpaque, the session's + 0x814) - nothing draws by it; on, the interface's windows are
// whole (100 %), off, as «Прозорість вікон» has them
TEST(options_card_opaque_windows) {
    CHECK(optionscard::windowOpacity(80, true) == 100);
    CHECK(optionscard::windowOpacity(80, false) == 80);
    CHECK(optionscard::windowOpacity(55, false) == 55);
}
