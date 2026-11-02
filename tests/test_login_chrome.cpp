// 3.0.11-beta.173 (the login concept): the login screen's bottom strip shared by its views - the server's status
// pill in the middle (core/login_chrome)
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "core/login_chrome.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(login_chrome_status_pill) {
    const std::string words = "Сервер працює · 157 гравців онлайн · пінг 52 мс";
    const loginchrome::Layout L = loginchrome::layout(1920, 1080, 1.0f, words);
    CHECK(L.y + L.h == 1080 && L.h > 0 && L.w == 1920);
    CHECK(std::abs(int(L.status.x + L.status.w / 2) - 960) <= 2);  // centred
    CHECK(L.status.y + L.status.h <= L.h - 20);                  // above the screen's edge
    CHECK(L.status.w > panel::wordsWidth(words, 14, false));
    const loginchrome::Layout none = loginchrome::layout(1920, 1080, 1.0f, "");
    CHECK(none.status.w == 0);  // no words: no pill
}

// 3.0.11-beta.174 (the login concept: the language right on the login screen): the strip's three parts - the views'
// switch at the left, the status in the middle, the language at the right (УКР / РУС / ENG); the mouse on each
TEST(login_chrome_bottom_bar_places) {
    const loginchrome::Layout L = loginchrome::layout(1920, 1080, 1.0f, "Сервер працює · пінг 52 мс");
    CHECK(L.langSeg[2].x + L.langSeg[2].w <= 1920 - 24 + 0.5f);
    CHECK(L.viewSeg[0].x >= 24 - 0.5f);
    CHECK(std::fabs(L.status.x + L.status.w / 2 - 960) <= 2);  // the status in the middle
    CHECK(L.viewSeg[2].x + L.viewSeg[2].w < L.status.x);      // the views left of the status
    CHECK(L.langSeg[0].x > L.status.x + L.status.w);          // the language right of it
    for (int i = 1; i < 3; ++i) CHECK(L.langSeg[i].x >= L.langSeg[i - 1].x + L.langSeg[i - 1].w - 0.5f);
    const panel::Box& b = L.langSeg[1];
    CHECK(loginchrome::hitTest(L, b.x + b.w / 2, b.y + b.h / 2) == loginchrome::Part::Lang1);
    CHECK(loginchrome::hitTest(L, L.status.x + 2, L.status.y + 2) == loginchrome::Part::None);
    loginchrome::Show sh;
    sh.lang = 2;
    const auto w = loginchrome::words(L, sh);
    CHECK(std::any_of(w.begin(), w.end(), [](const loginchrome::Words& x) { return x.text == "ENG" && x.bold; }));
}

// 3.0.11-beta.174 (the login concept's news line): the site's item now in a pill above the status, its «Усі новини»
// link after it; a long title cut to the screen; each its own part for the mouse
TEST(login_chrome_news_line) {
    loginchrome::Content c;
    c.status = "Сервер працює · пінг 52 мс";
    c.news = "Подія тижня: полювання на MVP";
    c.allNews = "Усі новини";
    const loginchrome::Layout L = loginchrome::layout(1920, 1080, 1.0f, c);
    CHECK(L.y + L.h == 1080);
    CHECK(L.news.w > 0 && std::fabs(L.news.x + L.news.w / 2 - 960) <= 2);  // centred
    CHECK(L.news.y >= 0 && L.news.y + L.news.h <= L.status.y - 8);         // above the status
    CHECK(L.newsLink.x >= L.newsText.x + L.newsText.w);                    // the link after the title
    CHECK(loginchrome::hitTest(L, L.newsText.x + 4, L.newsText.y + L.newsText.h / 2) == loginchrome::Part::News);
    CHECK(loginchrome::hitTest(L, L.newsLink.x + 4, L.newsLink.y + L.newsLink.h / 2) ==
          loginchrome::Part::AllNews);
    const loginchrome::Layout plain = loginchrome::layout(1920, 1080, 1.0f, c.status);  // no news: as before
    CHECK(plain.news.w == 0 && plain.h < L.h);
    CHECK(loginchrome::hitTest(plain, 960, 10) == loginchrome::Part::None);
    c.news = std::string(300, 'W');  // longer than the screen: cut with «…», the pill inside the screen
    const loginchrome::Layout cut = loginchrome::layout(1280, 720, 1.0f, c);
    CHECK(cut.news.x >= 24 - 0.5f && cut.news.x + cut.news.w <= 1280 - 24 + 0.5f);
    loginchrome::Show sh;
    sh.news = c.news;
    sh.allNews = c.allNews;
    const auto w = loginchrome::words(cut, sh);
    const auto title = std::find_if(w.begin(), w.end(), [](const loginchrome::Words& x) { return x.text[0] == 'W'; });
    CHECK(title != w.end());
    if (title != w.end()) {
        CHECK(title->text.size() < c.news.size() && title->text.substr(title->text.size() - 3) == "…");
        CHECK(panel::wordsWidth(title->text, title->px, false) <= cut.newsText.w + 0.5f);
    }
    CHECK(std::any_of(w.begin(), w.end(), [](const loginchrome::Words& x) { return x.text == "Усі новини"; }));
}

// 3.0.11-beta.177: the views' segments as wide as their words («Классический» wider than 96), one after another
TEST(login_chrome_view_segments_fit_their_words) {
    loginchrome::Content c;
    c.viewLabel = "Вид";
    c.viewNames[0] = "Классический";
    c.viewNames[1] = "Полка";
    c.viewNames[2] = "Продолжить";
    const loginchrome::Layout L = loginchrome::layout(1920, 1080, 1.0f, c);
    CHECK(L.viewSeg[0].w >= panel::wordsWidth(c.viewNames[0], 13.5f, true) + 20);
    CHECK(L.viewSeg[1].w >= 96 - 0.5f);
    for (int i = 1; i < 3; ++i) CHECK(std::fabs(L.viewSeg[i].x - (L.viewSeg[i - 1].x + L.viewSeg[i - 1].w)) < 0.5f);
    CHECK(L.viewSeg[0].x > L.viewLabel.x + L.viewLabel.w - 0.5f);
}
