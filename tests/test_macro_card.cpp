// 3.0.11-beta.169 (task 8 of the remaining windows: the chat's macros, Alt+M): the game's macro window (UI window
// 0x56) as a card (core/macro_card) - ten rows «Alt+1» ... «Alt+0», each the game's own field (typed as before, its
// words and caret shown), «Зберегти» the window's own Enter (it writes them), ✕ its close
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/macro_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

// The window's own (FUN_004534c0, its create FUN_00452ec0): ten fields from +0x64 (4 apart), Enter (its message 0)
// writes them, 6 / 0x87 closes it
TEST(macro_card_the_windows_fields) {
    CHECK(macrocard::kFields == 10 && macrocard::kFirstField == 0x64);
    CHECK(macrocard::kSaveMsg == 0 && macrocard::kCloseButton == 0x87);
    CHECK(macrocard::keyName(0) == "Alt+1" && macrocard::keyName(8) == "Alt+9" && macrocard::keyName(9) == "Alt+0");
}

TEST(macro_card_places_and_the_mouse) {
    const macrocard::Layout L = macrocard::layout(1920, 1080, 1.0f);
    CHECK(L.fields.size() == 10 && L.fields[9].y > L.fields[0].y && L.save.y > L.fields[9].y);
    using P = macrocard::Part;
    const macrocard::Hit f = macrocard::hitTest(L, mid(L.fields[4]).first, mid(L.fields[4]).second);
    CHECK(f.part == P::Field && f.index == 4);
    CHECK(macrocard::hitTest(L, mid(L.save).first, mid(L.save).second).part == P::Save);
    CHECK(macrocard::hitTest(L, mid(L.close).first, mid(L.close).second).part == P::Close);
    CHECK(macrocard::hitTest(L, -5, -5).part == P::None);
}

TEST(macro_card_words_and_picture) {
    const macrocard::Layout L = macrocard::layout(1920, 1080, 1.0f);
    std::vector<std::string> texts(10);
    texts[0] = "/!";
    texts[3] = "Heal please";
    macrocard::Show sh;
    sh.focus = 3;
    sh.caret = true;
    macrocard::Texts t;
    t.title = "Макроси чату";
    t.hint = "Alt+цифра пише рядок у чат";
    t.save = "Зберегти";
    const std::vector<macrocard::Words> w = macrocard::words(L, texts, sh, t);
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const macrocard::Words& x) { return x.text == s; });
    }
    ;
    CHECK(has("Макроси чату") && has("Alt+1") && has("Alt+0") && has("/!") && has("Heal please|"));
    CHECK(has("Зберегти"));
}
