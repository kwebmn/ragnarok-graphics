// 3.0.11-beta.177 (the login concept's views): «Класичний», «Полиця», «Продовжити» (core/login_view) and
// «Продовжити»'s panel (core/login_continue) - the last account's character big, «Грати», the links under it
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/login_continue.hpp"
#include "core/login_view.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(login_view_tokens) {
    using V = loginview::View;
    for (const V v : {V::Classic, V::Shelf, V::Continue}) CHECK(loginview::parse(loginview::token(v)) == v);
    CHECK(std::string(loginview::token(V::Classic)) == "classic" && std::string(loginview::token(V::Shelf)) == "shelf");
    CHECK(std::string(loginview::token(V::Continue)) == "continue");
    CHECK(!loginview::parse("bookshelf").has_value() && !loginview::parse("").has_value());
    CHECK(loginview::parse("Shelf") == V::Shelf);  // any case
}

TEST(login_continue_places) {
    const logincontinue::Layout L = logincontinue::layout(1920, 1080, 1.0f);
    CHECK(std::abs(L.x + L.w / 2 - 960) <= 2 && std::abs(L.y + L.h / 2 - 540) <= 2);
    CHECK(L.art.x <= 0.5f && std::fabs(L.art.w - L.art.h) < 0.5f && L.art.h >= float(L.h) - 0.5f);
    CHECK(L.play.h >= 56 - 0.5f && L.play.x >= L.art.x + L.art.w);
    CHECK(L.otherChar.y > L.play.y + L.play.h - 0.5f && L.change.x > L.otherChar.x && L.create.x > L.change.x);
    using P = logincontinue::Part;
    const auto at = [&](const panel::Box& b) { return logincontinue::hitTest(L, b.x + 3, b.y + b.h / 2); };
    CHECK(at(L.play) == P::Play && at(L.otherChar) == P::OtherChar && at(L.change) == P::Change);
    CHECK(at(L.create) == P::Create && logincontinue::hitTest(L, -3, -3) == P::None);
    logincontinue::Account a;
    a.name = "Raphael";
    a.job = "High Priest";
    a.level = 99;
    a.map = "prontera";
    a.login = "kweb";
    logincontinue::Texts t;
    t.heading = "Продовжити гру";
    t.play = "Грати";
    t.loginWord = "логін {1}";
    t.otherChar = "Інший персонаж";
    t.change = "Змінити акаунт";
    t.create = "Створити акаунт";
    const auto w = logincontinue::words(L, a, logincontinue::Show{}, t);
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const logincontinue::Words& x) { return x.text == s; });
    };
    CHECK(has("Raphael") && has("High Priest · 99 · Prontera · логін kweb") && has("Продовжити гру"));
    CHECK(has("Грати") && has("Інший персонаж") && has("Змінити акаунт") && has("Створити акаунт"));
}

// 3.0.11-beta.196 (the tester: «обидві текстовки (Грати та enter) по центру, а enter мала би бути по нижньому краю
// тексту "грати"»): «Enter» on «Грати»'s line - the words are drawn centred in their boxes (GDI's DT_VCENTER over
// Plex's 1025 / 275 cell: the line 0.375 of a size under the middle), so the smaller word's box drops by 0.375 of the
// sizes' difference
TEST(login_continue_enter_on_plays_line) {
    CHECK(std::fabs(panel::baselineDrop(19, 12.5f) - 0.375f * 6.5f) < 1e-4f);
    CHECK(panel::baselineDrop(13, 13) == 0.0f);
    const logincontinue::Layout L = logincontinue::layout(1920, 1080, 1.0f);
    logincontinue::Texts t;
    t.play = "Грати";
    t.enterKey = "Enter";
    const auto w = logincontinue::words(L, logincontinue::Account{}, logincontinue::Show{}, t);
    const logincontinue::Words* play = nullptr;
    const logincontinue::Words* enter = nullptr;
    for (const logincontinue::Words& x : w) {
        if (x.text == "Грати") play = &x;
        if (x.text == "Enter") enter = &x;
    }
    CHECK(play && enter);
    if (play && enter) {
        const float line = play->box.y + play->box.h / 2 + 0.375f * play->px;     // «Грати»'s
        const float its = enter->box.y + enter->box.h / 2 + 0.375f * enter->px;  // «Enter»'s
        CHECK(std::fabs(line - its) < 0.01f);
    }
}
