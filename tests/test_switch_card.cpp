// 3.0.11-beta.168 (task 5 of the remaining windows: the whisper's settings, the friends' window's «Налаштування»): a
// card of switches (core/switch_card) - its title, a row a switch (its words, its state), ✕; the whisper settings
// window (UI window 0x44) the first to use it
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/switch_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

TEST(switch_card_places_and_the_mouse) {
    const switchcard::Layout L = switchcard::layout(1920, 1080, 1.0f, 3);
    CHECK(L.toggles.size() == 3 && L.labels.size() == 3 && L.toggles[2].y > L.toggles[0].y);
    CHECK(L.x + L.w / 2 > 900 && L.x + L.w / 2 < 1020);
    using P = switchcard::Part;
    const switchcard::Hit t = switchcard::hitTest(L, mid(L.toggles[1]).first, mid(L.toggles[1]).second);
    CHECK(t.part == P::Toggle && t.index == 1);
    const switchcard::Hit r = switchcard::hitTest(L, mid(L.labels[2]).first, mid(L.labels[2]).second);
    CHECK(r.part == P::Toggle && r.index == 2);  // its words flip it too
    CHECK(switchcard::hitTest(L, mid(L.close).first, mid(L.close).second).part == P::Close);
    CHECK(switchcard::hitTest(L, -5, -5).part == P::None);
}

TEST(switch_card_words_and_picture) {
    const switchcard::Layout L = switchcard::layout(1920, 1080, 1.0f, 2);
    switchcard::Texts t;
    t.title = "Налаштування шепоту";
    t.labels = {"Окреме вікно для незнайомих", "Звук нового шепоту"};
    t.hint = "Шепіт друзів";
    std::vector<bool> on{true, false};
    const std::vector<switchcard::Words> w = switchcard::words(L, on, switchcard::Show{}, t);
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const switchcard::Words& x) { return x.text == s; });
    }
    ;
    CHECK(has("Налаштування шепоту") && has("Окреме вікно для незнайомих") && has("Звук нового шепоту"));
}
