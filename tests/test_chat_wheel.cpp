// 3.0.11-beta.214 (the owner: «посмотри, как сделать удобно начинать общение с геймпада»; answers: 1 «придумай»,
// 2 «давай», 4 «можно», 5 «надо настраивать и поставь те по умолчанию которые по умолчанию в игре»): the chat wheel
// (core/chat_wheel) - the Talk button held opens it, the right stick points at a sector, letting go sends it
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "core/chat_wheel.hpp"
#include "test.hpp"

using namespace uagfx;

// The right stick (x right, y up) at a sector: the first at the top, then clockwise; none in the middle
TEST(chat_wheel_sector_from_the_stick) {
    CHECK_EQ(chatwheel::sectorAt(0, 1, 8), 0);
    CHECK_EQ(chatwheel::sectorAt(0.7f, 0.7f, 8), 1);
    CHECK_EQ(chatwheel::sectorAt(1, 0, 8), 2);
    CHECK_EQ(chatwheel::sectorAt(0, -1, 8), 4);
    CHECK_EQ(chatwheel::sectorAt(-1, 0, 8), 6);
    CHECK_EQ(chatwheel::sectorAt(-0.1f, 0.99f, 8), 0);  // just left of the top
    CHECK_EQ(chatwheel::sectorAt(0.1f, 0.1f, 8), -1);    // the dead middle
    CHECK_EQ(chatwheel::sectorAt(0, 1, 10), 0);
    CHECK_EQ(chatwheel::sectorAt(0.95f, 0.2f, 10), 2);
    CHECK_EQ(chatwheel::sectorAt(-0.95f, 0.2f, 10), 8);
    CHECK_EQ(chatwheel::sectorAt(0, 1, 0), -1);
}

// The phrases: the configured ones, an empty one its default (the interface's words)
TEST(chat_wheel_phrases) {
    std::array<std::string, chatwheel::kPhrases> mine{};
    mine[1] = "  Дякую всім!  ";
    std::array<std::string, chatwheel::kPhrases> defaults{};
    for (int i = 0; i < chatwheel::kPhrases; ++i) defaults[size_t(i)] = "d" + std::to_string(i);
    const std::vector<std::string> p = chatwheel::phraseList(mine, defaults);
    CHECK_EQ(int(p.size()), chatwheel::kPhrases);
    CHECK(p[0] == "d0" && p[1] == "Дякую всім!" && p[7] == "d7");
}

// The emotions: the configured commands (by spaces, ten at most), none - the game's own ten
TEST(chat_wheel_emotes) {
    const std::vector<std::string> game = {"/!", "/?", "/ho", "/lv", "/swt", "/ic", "/an", "/ag", "/$", "/..."};
    CHECK((chatwheel::emoteList("", game) == game));
    CHECK((chatwheel::emoteList("  /heh   /kis /gg ", game) == std::vector<std::string>{"/heh", "/kis", "/gg"}));
    CHECK_EQ(int(chatwheel::emoteList("/a /b /c /d /e /f /g /h /i /j /k /l", game).size()), chatwheel::kEmotes);
    CHECK((chatwheel::emoteList("hello /ok", game) == std::vector<std::string>{"/ok"}));  // a command only
}

// At the screen's middle; a label a sector, round the middle; the pages over it, the channels under it
TEST(chat_wheel_layout) {
    const chatwheel::Layout L = chatwheel::layout(8, 1920, 1080, 1.0f);
    CHECK(L.x >= 0 && L.y >= 0 && L.x + L.w <= 1920 && L.y + L.h <= 1080);
    CHECK(std::abs(float(L.x) + L.cx - 960.0f) < 2.0f);
    CHECK_EQ(int(L.labels.size()), 8);
    CHECK(L.labels[0].y + L.labels[0].h / 2 < L.cy && L.labels[4].y + L.labels[4].h / 2 > L.cy);  // top, bottom
    CHECK(L.labels[2].x + L.labels[2].w / 2 > L.cx && L.labels[6].x + L.labels[6].w / 2 < L.cx);  // right, left
    CHECK(L.pages[0].y + L.pages[0].h <= L.cy - L.outer + 1 && L.channels[0].y >= L.cy + L.outer - 1);
    CHECK(L.inner > 0 && L.outer > L.inner);
    const chatwheel::Layout ten = chatwheel::layout(10, 1280, 720, 1.0f);
    CHECK_EQ(int(ten.labels.size()), 10);
    CHECK(ten.y >= 0 && ten.y + ten.h <= 720);
}

// The words: each item, the page and the channel chosen marked, the buttons' hints
TEST(chat_wheel_words) {
    const chatwheel::Layout L = chatwheel::layout(8, 1920, 1080, 1.0f);
    chatwheel::Texts t;
    t.pages[0] = "Фрази";
    t.pages[1] = "Емоції";
    t.channels[0] = "Усім";
    t.channels[1] = "Групі";
    t.channels[2] = "Гільдії";
    t.keyboard = "Клавіатура";
    t.voice = "Голос";
    t.hint = "Відпустіть — надіслати";
    std::vector<std::string> items;
    for (int i = 0; i < 8; ++i) items.push_back("p" + std::to_string(i));
    const std::vector<chatwheel::Words> w =
        chatwheel::words(L, items, 3, chatwheel::Page::Phrases, chatwheel::Channel::Party, t);
    const auto find = [&](const std::string& x) {
        return std::find_if(w.begin(), w.end(), [&](const chatwheel::Words& y) { return y.text == x; });
    };
    CHECK(find("p0") != w.end() && find("p7") != w.end());
    CHECK(find("p3")->rgb != find("p2")->rgb);  // the one pointed at
    CHECK(find("Групі")->rgb != find("Усім")->rgb);
    CHECK(find("Фрази")->rgb != find("Емоції")->rgb);
    CHECK(find("Клавіатура") != w.end() && find("Голос") != w.end() && find("Відпустіть — надіслати") != w.end());
}
