// 3.0.11-beta.150 (the owner: «сделай все без остановки, в инлайн режиме» - task 2 of the remaining windows): the
// game's small windows as one card (core/ask_card) - a trade's, a party's, a guild's, a friend's request with accept
// and decline, an NPC's number or text with its field and OK
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/ask_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

askcard::Texts texts() {
    askcard::Texts t;
    t.accept = "Прийняти";
    t.decline = "Відмовити";
    t.ok = "Готово";
    t.declineKey = "Esc";
    return t;
}

bool has(const std::vector<askcard::Words>& w, const std::string& text) {
    return std::any_of(w.begin(), w.end(), [&](const askcard::Words& x) { return x.text == text; });
}

}  // namespace

// A request: accept and decline, the game's own buttons 0x76 and 0x77; an input: its OK (0x76)
TEST(ask_card_buttons_by_kind) {
    using B = askcard::Button;
    for (const askcard::Kind k : {askcard::Kind::Trade, askcard::Kind::Party, askcard::Kind::Guild,
                                  askcard::Kind::Friend}) {
        CHECK(askcard::request(k));
        CHECK((askcard::buttons(k) == std::vector<B>{B::Accept, B::Decline}));
    }
    CHECK(!askcard::request(askcard::Kind::Number) && !askcard::request(askcard::Kind::Text));
    CHECK((askcard::buttons(askcard::Kind::Number) == std::vector<B>{B::Ok}));
    CHECK(askcard::gameButton(B::Accept) == 0x76 && askcard::gameButton(B::Decline) == 0x77);
    CHECK(askcard::gameButton(B::Ok) == 0x76);
}

// The card at the screen's middle across, in its upper part; its buttons in a row under the words, the mouse on each;
// an input's field over its button
TEST(ask_card_places_and_the_mouse) {
    askcard::Card c;
    c.kind = askcard::Kind::Trade;
    c.title = "Обмін";
    c.message = "Tester пропонує обмін";
    const askcard::Layout L = askcard::layout(c, 1920, 1080, 1.0f, texts());
    CHECK(std::abs(L.x + L.w / 2 - 960) <= 2 && L.y > 100 && L.y + L.h < 1080 / 2 + 60);
    CHECK(L.buttons.size() == 2 && L.buttons[0].y > L.message.y && L.buttons[1].x > L.buttons[0].x);
    const panel::Box& b = L.buttons[1];
    CHECK(askcard::buttonAt(L, b.x + b.w / 2, b.y + b.h / 2) == 1 && askcard::buttonAt(L, -5, -5) == -1);
    CHECK(L.field.w == 0);
    askcard::Card n;
    n.kind = askcard::Kind::Number;
    n.title = "Введіть число";
    n.field = "1250";
    const askcard::Layout N = askcard::layout(n, 1920, 1080, 1.0f, texts());
    CHECK(N.buttons.size() == 1 && N.field.w > 100 && N.field.y + N.field.h <= N.buttons[0].y);
}

// Its words: the title, the message, the buttons' and the decline's key; an input's field as the game's has it, its
// caret blinking as a bar after it; the glass painted
TEST(ask_card_words_and_picture) {
    askcard::Card c;
    c.kind = askcard::Kind::Party;
    c.title = "Група";
    c.message = "Запрошення в групу Morroc Hunters";
    askcard::Show sh;
    const askcard::Layout L = askcard::layout(c, 1920, 1080, 1.0f, texts());
    const std::vector<askcard::Words> w = askcard::words(L, c, sh, texts());
    CHECK(has(w, "Група") && has(w, "Запрошення в групу Morroc Hunters") && has(w, "Прийняти") && has(w, "Відмовити"));
    CHECK(has(w, "Esc"));
    askcard::Card n;
    n.kind = askcard::Kind::Text;
    n.title = "Введіть текст";
    n.field = "prontera";
    n.caret = true;
    const askcard::Layout N = askcard::layout(n, 1920, 1080, 1.0f, texts());
    const std::vector<askcard::Words> wn = askcard::words(N, n, sh, texts());
    CHECK(has(wn, "prontera|") && has(wn, "Готово") && !has(wn, "Відмовити"));
    n.caret = false;
    CHECK(has(askcard::words(N, n, sh, texts()), "prontera"));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    askcard::paint(p, L, c, sh);
    CHECK((p.px[size_t(L.h / 2) * size_t(L.w) + size_t(L.w / 2)] >> 24) > 0x80);
}
