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
    t.yes = "Так";
    t.no = "Ні";
    t.cancel = "Скасувати";
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

// 3.0.11-beta.151 (task 3 of the remaining windows, the login's part): the game's messages (UIWindowMgr::ErrorMsg -
// «Unregistered ID ...», «Disconnected from server») on the same card: OK (0x76), Yes and No (0x79, 0x7a), OK and
// Cancel (0x76, 0x77), and the wait («Please wait...») with no button; a long message as many rows as it needs
TEST(ask_card_the_games_messages) {
    using B = askcard::Button;
    CHECK((askcard::buttons(askcard::Kind::Message) == std::vector<B>{B::Ok}));
    CHECK((askcard::buttons(askcard::Kind::Question) == std::vector<B>{B::Yes, B::No}));
    CHECK((askcard::buttons(askcard::Kind::Choice) == std::vector<B>{B::Ok, B::Cancel}));
    CHECK(askcard::buttons(askcard::Kind::Wait).empty());
    CHECK(askcard::gameButton(B::Yes) == 0x79 && askcard::gameButton(B::No) == 0x7a);
    CHECK(askcard::gameButton(B::Cancel) == 0x77);
    for (const askcard::Kind k : {askcard::Kind::Message, askcard::Kind::Question, askcard::Kind::Choice,
                                  askcard::Kind::Wait})
        CHECK(!askcard::request(k) && !askcard::input(k));
    CHECK(askcard::input(askcard::Kind::Number) && askcard::input(askcard::Kind::Text));
    askcard::Card m;
    m.kind = askcard::Kind::Message;
    m.message = "Unregistered ID. Please make sure you have a registered account and you have correctly typed in "
                "the user ID.";
    m.messageLines = 1;
    const askcard::Layout one = askcard::layout(m, 1920, 1080, 1.0f, texts());
    m.messageLines = 4;
    const askcard::Layout four = askcard::layout(m, 1920, 1080, 1.0f, texts());
    CHECK(four.message.h >= 19 * 4 - 0.5f && four.message.h > one.message.h + 30);  // a row 19, two at least
    CHECK(four.buttons.size() == 1 && four.field.w == 0);
    askcard::Card w;
    w.kind = askcard::Kind::Wait;
    w.message = "Please wait...";
    const askcard::Layout L = askcard::layout(w, 1920, 1080, 1.0f, texts());
    CHECK(L.buttons.empty() && L.message.w > 0);
    const std::vector<askcard::Words> words = askcard::words(L, w, askcard::Show{}, texts());
    CHECK(has(words, "Please wait..."));
}

// 3.0.11-beta.155 (task 4 of the remaining windows: the game's «how many» window, UI window 0xF - a stack into the
// storage, the cart, the bag, a trade, a shop): an input with OK and «Скасувати», the number field and three quick
// choices under it - one, half, all of the most it may take (each sets the field and OKs)
TEST(ask_card_the_games_count) {
    using B = askcard::Button;
    CHECK(askcard::input(askcard::Kind::Count) && !askcard::request(askcard::Kind::Count));
    CHECK((askcard::buttons(askcard::Kind::Count) == std::vector<B>{B::Ok, B::Cancel}));
    askcard::Card c;
    c.kind = askcard::Kind::Count;
    c.title = "На склад";
    c.message = "Скільки штук? Найбільше — 25";
    c.field = "25";
    c.most = 25;
    askcard::Texts t = texts();
    t.quick[0] = "1";
    t.quick[1] = "Половина";
    t.quick[2] = "Усі";
    const askcard::Layout L = askcard::layout(c, 1920, 1080, 1.0f, t);
    CHECK(L.quick.size() == 3);
    CHECK(L.quick[0].y >= L.field.y + L.field.h && L.buttons[0].y >= L.quick[0].y + L.quick[0].h);
    const auto mid = [](const panel::Box& b) { return std::make_pair(b.x + b.w / 2, b.y + b.h / 2); };
    CHECK(askcard::quickAt(L, mid(L.quick[2]).first, mid(L.quick[2]).second) == 2);
    CHECK(askcard::quickAt(L, mid(L.field).first, mid(L.field).second) == -1);
    CHECK(askcard::quickValue(askcard::Quick::One, 25) == 1);
    CHECK(askcard::quickValue(askcard::Quick::Half, 25) == 12);
    CHECK(askcard::quickValue(askcard::Quick::All, 25) == 25);
    CHECK(askcard::quickValue(askcard::Quick::Half, 1) == 1);
    const std::vector<askcard::Words> w = askcard::words(L, c, askcard::Show{}, t);
    CHECK(has(w, "Усі") && has(w, "Половина") && has(w, "Скасувати") && has(w, "Готово"));
    askcard::Card npc = c;  // an NPC's number: no quick choices
    npc.kind = askcard::Kind::Number;
    CHECK(askcard::layout(npc, 1920, 1080, 1.0f, t).quick.empty());
}
