// 3.0.11-beta.173 (the login concept): the login screen's bottom strip shared by its views - the server's status
// pill in the middle (core/login_chrome)
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
