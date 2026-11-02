// 3.0.11-beta.176 (the login concept's «Полиця»): the login screen's shelf of accounts (core/login_shelf) - a card an
// account in a row under the game's logo, «Додати акаунт» after them, four in sight with ◀ ▶ beyond, the buttons under
// the row; the first launch's classic card; a card's menu
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/login_shelf.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

loginshelf::Texts texts() {
    loginshelf::Texts t;
    t.title = "Оберіть акаунт";
    t.titleHint = "— один клік, і ви в грі";
    t.play = "Грати";
    t.enterKey = "Enter";
    t.loginWord = "логін {1}";
    t.add = "Додати акаунт";
    t.addHint = "логін і пароль один раз, далі — в один клік";
    t.other = "Інший акаунт";
    t.create = "Створити акаунт";
    t.exit = "Вийти";
    t.refused = "Пароль не підходить";
    t.busy = "Вже в грі";
    t.noChar = "Ще не грали";
    return t;
}

}  // namespace

TEST(login_shelf_places) {
    const loginshelf::Layout L = loginshelf::layout(1920, 1080, 1.0f, 3, 0);
    CHECK(L.cards.size() == 3 && L.add.w > 0);
    const float left = float(L.x) + L.cards[0].x, right = float(L.x) + L.add.x + L.add.w;
    CHECK(std::fabs((left + right) / 2 - 960) <= 2);  // the row centred
    const float gap = L.cards[1].x - (L.cards[0].x + L.cards[0].w);
    CHECK(gap > 0 && std::fabs(L.cards[2].x - (L.cards[1].x + L.cards[1].w) - gap) < 0.5f);
    CHECK(std::fabs(L.add.x - (L.cards[2].x + L.cards[2].w) - gap) < 0.5f);
    CHECK(float(L.y) + L.cards[0].y >= 300);  // under the logo
    CHECK(L.other.y > L.cards[0].y + L.cards[0].h && L.exit.x > L.create.x && L.create.x > L.other.x);
    CHECK(L.left.w == 0 && L.right.w == 0);  // nothing beyond
    using P = loginshelf::Part;
    const auto mid = [](const panel::Box& b) { return std::pair<float, float>{b.x + b.w / 2, b.y + b.h / 2}; };
    const loginshelf::Hit h = loginshelf::hitTest(L, mid(L.cards[1]).first, mid(L.cards[1]).second);
    CHECK(h.part == P::Card && h.index == 1);
    CHECK(loginshelf::hitTest(L, mid(L.add).first, mid(L.add).second).part == P::Add);
    CHECK(loginshelf::hitTest(L, mid(L.exit).first, mid(L.exit).second).part == P::Exit);
    const panel::Box dots = loginshelf::menuButton(L, 0);
    const loginshelf::Hit m = loginshelf::hitTest(L, dots.x + dots.w / 2, dots.y + dots.h / 2);
    CHECK(m.part == P::Menu && m.index == 0);
    // six accounts: four in sight, the arrows; from the third on - the fifth and sixth with «Додати акаунт»
    const loginshelf::Layout six = loginshelf::layout(1920, 1080, 1.0f, 6, 0);
    CHECK(six.cards.size() == 4 && six.add.w == 0 && six.right.w > 0 && six.left.w == 0);
    CHECK(loginshelf::hitTest(six, mid(six.right).first, mid(six.right).second).part == P::Right);
    const loginshelf::Layout later = loginshelf::layout(1920, 1080, 1.0f, 6, 3);
    CHECK(later.cards.size() == 3 && later.add.w > 0 && later.left.w > 0 && later.right.w == 0);
    CHECK(loginshelf::hitTest(later, mid(later.cards[0]).first, mid(later.cards[0]).second).index == 3);
}

TEST(login_shelf_first_launch_is_classic) {
    CHECK(loginshelf::classicInstead(0));
    CHECK(!loginshelf::classicInstead(1));
}

// A card's menu: «Одразу в гру», «Зберігати пароль», the label, the login and password, its place, off the shelf; a
// refused password said on the card in red
TEST(login_shelf_menu_items) {
    loginshelf::Card c;
    c.name = "Raphael";
    c.job = "High Priest";
    c.level = 99;
    c.map = "prontera";
    c.login = "kweb";
    using M = loginshelf::MenuItem;
    CHECK((loginshelf::menu(c) ==
           std::vector<M>{M::AutoEnter, M::KeepPassword, M::Label, M::Edit, M::MoveLeft, M::MoveRight, M::Remove}));
    const loginshelf::Layout L = loginshelf::layout(1920, 1080, 1.0f, 1, 0);
    const auto w = loginshelf::words(L, {c}, loginshelf::Show{}, texts());
    const auto has = [&](const std::vector<loginshelf::Words>& ws, const std::string& t) {
        return std::any_of(ws.begin(), ws.end(), [&](const loginshelf::Words& x) { return x.text == t; });
    };
    CHECK(has(w, "Raphael") && has(w, "High Priest · 99 · Prontera") && has(w, "логін kweb"));
    CHECK(!has(w, "Пароль не підходить"));
    c.refused = true;
    const auto r = loginshelf::words(L, {c}, loginshelf::Show{}, texts());
    const auto red =
        std::find_if(r.begin(), r.end(), [](const loginshelf::Words& x) { return x.text == "Пароль не підходить"; });
    CHECK(red != r.end() && red->rgb == 0xFF5A4E);
    loginshelf::Card fresh;  // no character yet: its login big, «Ще не грали»
    fresh.login = "newbie";
    const auto f = loginshelf::words(L, {fresh}, loginshelf::Show{}, texts());
    CHECK(has(f, "newbie") && has(f, "Ще не грали"));
}

// The menu over its card, a row an item; the mouse on each
TEST(login_shelf_menu_places) {
    const loginshelf::Layout L = loginshelf::layout(1920, 1080, 1.0f, 2, 0);
    const loginshelf::MenuLayout M = loginshelf::menuLayout(L, 1, 7);
    CHECK(M.rows.size() == 7);
    CHECK(M.x >= L.cards[1].x - 0.5f && M.x + M.w <= L.cards[1].x + L.cards[1].w + 0.5f);
    CHECK(M.y + M.h <= L.cards[1].y + L.cards[1].h + 0.5f);
    CHECK(loginshelf::menuHit(M, M.rows[3].x + 5, M.rows[3].y + M.rows[3].h / 2) == 3);
    CHECK(loginshelf::menuHit(M, M.x - 5, M.y + 5) == -1);
}

// 3.0.11-beta.179 (the login concept: the pad on the login screen): ◀ ▶ go along the cards and stop at their ends
TEST(login_shelf_pad_moves) {
    CHECK(loginshelf::padMove(0, 3, -1) == 0);
    CHECK(loginshelf::padMove(0, 3, +1) == 1);
    CHECK(loginshelf::padMove(2, 3, +1) == 2);
    CHECK(loginshelf::padMove(1, 3, -1) == 0);
    CHECK(loginshelf::padMove(0, 0, +1) == 0);  // no cards
}

// 3.0.11-beta.181: «Персонажі» - a row a login, up to three character cards in it and «Новий персонаж» after them
TEST(login_shelf_character_rows) {
    const loginshelf::CharLayout L = loginshelf::characterRows(1920, 1080, 1.0f, {2, 1});
    CHECK(L.rows.size() == 2);
    CHECK(L.rows[0].cards.size() == 2 && L.rows[0].newChar.w > 0 && L.rows[1].cards.size() == 1);
    CHECK(L.rows[0].newChar.x > L.rows[0].cards[1].x + L.rows[0].cards[1].w - 0.5f);
    CHECK(L.rows[1].label.y > L.rows[0].label.y);
    CHECK(std::fabs(float(L.x) + float(L.w) / 2 - 960) <= 2);
    const loginshelf::CharHit h = loginshelf::characterHit(L, L.rows[1].cards[0].x + 5, L.rows[1].cards[0].y + 5);
    CHECK(h.row == 1 && h.card == 0 && !h.newChar);
    const loginshelf::CharHit n = loginshelf::characterHit(L, L.rows[0].newChar.x + 5, L.rows[0].newChar.y + 5);
    CHECK(n.row == 0 && n.newChar);
    const loginshelf::CharLayout full = loginshelf::characterRows(1920, 1080, 1.0f, {5});
    CHECK(full.rows[0].cards.size() == 3);  // three at most in a row
}

// 3.0.11-beta.181: signed in to the site - «Персонажі ›» at the title's right, «+ Новий ігровий логін» and
// «Прив'язати наявний» under the buttons; a site login with no password here says it is asked once
TEST(login_shelf_site_parts) {
    const loginshelf::Layout plain = loginshelf::layout(1920, 1080, 1.0f, 2, 0);
    const loginshelf::Layout L = loginshelf::layout(1920, 1080, 1.0f, 2, 0, true);
    CHECK(plain.chars.w == 0 && plain.newLogin.w == 0 && L.chars.w > 0 && L.newLogin.w > 0 && L.linkLogin.w > 0);
    CHECK(L.newLogin.y > L.other.y + L.other.h - 0.5f && L.linkLogin.x > L.newLogin.x);
    using P = loginshelf::Part;
    CHECK(loginshelf::hitTest(L, L.chars.x + 4, L.chars.y + L.chars.h / 2).part == P::Chars);
    CHECK(loginshelf::hitTest(L, L.newLogin.x + 4, L.newLogin.y + L.newLogin.h / 2).part == P::NewLogin);
    CHECK(loginshelf::hitTest(L, L.linkLogin.x + 4, L.linkLogin.y + L.linkLogin.h / 2).part == P::LinkLogin);
    loginshelf::Card c;
    c.login = "kweb_main";
    c.hasPassword = false;
    c.fromSite = true;
    loginshelf::Texts t;
    t.noPassword = "Пароль — ввести один раз";
    t.noChar = "Ще не грали";
    const auto w = loginshelf::words(L, {c}, loginshelf::Show{}, t);
    CHECK(std::any_of(w.begin(), w.end(),
                      [](const loginshelf::Words& x) { return x.text == "Пароль — ввести один раз"; }));
}

// 3.0.11-beta.183 (R-BFQFDU): «Інший акаунт», «Створити акаунт», «Вийти» and the site's links stand on the shelf's
// dark plates - white words over a faint white ghost were lost on a bright wallpaper
TEST(login_shelf_buttons_on_plates) {
    const loginshelf::Layout L = loginshelf::layout(1920, 1080, 1.0f, 2, 0, true);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0);
    loginshelf::Texts t = texts();
    t.chars = "Персонажі ›";
    t.newLogin = "+ Новий ігровий логін";
    t.linkLogin = "Прив'язати наявний логін";
    loginshelf::paint(p, L, {loginshelf::Card{}, loginshelf::Card{}}, loginshelf::Show{}, t);
    const auto alpha = [&](float x, float y) { return int(p.px[size_t(y) * size_t(p.width) + size_t(x)] >> 24); };
    for (const panel::Box& b : {L.other, L.create, L.exit}) CHECK(alpha(b.x + b.w / 2, b.y + b.h / 2) >= 170);
    // the links' plates round their words: «+ Новий ігровий логін» to the right, «Прив'язати наявний логін» to the left
    CHECK(alpha(L.newLogin.x + L.newLogin.w - 20, L.newLogin.y + L.newLogin.h / 2) >= 170);
    CHECK(alpha(L.linkLogin.x + 20, L.linkLogin.y + L.linkLogin.h / 2) >= 170);
    CHECK(alpha(L.chars.x + L.chars.w - 20, L.chars.y + L.chars.h / 2) >= 170);
    CHECK(alpha(L.newLogin.x + 2, L.newLogin.y + L.newLogin.h / 2) == 0);  // not the whole box: just round the words
}

// 3.0.11-beta.181: «Персонажі»'s words - each row's login, each card's name, job and level, map; «+ Новий персонаж»;
// a login with no password here says so on its row
TEST(login_shelf_character_words) {
    std::vector<loginshelf::CharLogin> logins(2);
    logins[0].login = "kweb";
    logins[0].hasPassword = true;
    logins[0].chars.push_back(siteapi::SiteChar{"Raphael", "High Priest", "prontera", 4009, 99, 2, false});
    logins[1].login = "kweb_alt";
    const loginshelf::CharLayout L = loginshelf::characterRows(1920, 1080, 1.0f, {1, 0});
    loginshelf::CharTexts t;
    t.title = "Персонажі";
    t.back = "‹ Акаунти";
    t.newChar = "+ Новий персонаж";
    t.noPassword = "пароль — ввести один раз";
    const auto w = loginshelf::characterWords(L, logins, loginshelf::CharShow{}, t);
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const loginshelf::Words& x) { return x.text == s; });
    };
    CHECK(has("kweb") && has("Raphael") && has("High Priest · 99") && has("Prontera") && has("+ Новий персонаж"));
    CHECK(has("kweb_alt · пароль — ввести один раз") && has("‹ Акаунти") && has("Персонажі"));
    panel::Picture p;
    panel::blank(p, L.w, L.h);
    loginshelf::paintCharacters(p, L, logins, loginshelf::CharShow{});
    const panel::Box& c = L.rows[0].cards[0];
    CHECK((p.px[size_t(c.y + c.h / 2) * size_t(L.w) + size_t(c.x + c.w - 10)] >> 24) > 0x80);  // the card's ground
}

// 3.0.11-beta.181: a site login's menu has «Змінити пароль гри»; one only on the site (not on this computer's shelf)
// - its password typed once and the game password changed, nothing of this computer's
TEST(login_shelf_site_menu) {
    using M = loginshelf::MenuItem;
    loginshelf::Card c;
    c.fromSite = true;
    const std::vector<M> both = loginshelf::menu(c);
    CHECK(both.size() == 8 && both.back() == M::GamePassword);
    c.localRow = false;
    CHECK((loginshelf::menu(c) == std::vector<M>{M::Edit, M::GamePassword}));
}
