#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::loginchrome {

// 3.0.11-beta.173 (the login concept): the login screen's strip at the screen's foot, shared by its views - the
// server's status pill in the middle (core/server_status says its words). The parts that need no Windows; the words
// by GDI (win/login_chrome).
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;          // the strip in the target's pixels
    panel::Box status, statusDot, statusText;  // in the strip; 0 wide when there are no words
    // 3.0.11-beta.174: the views' switch at the left (its words' label before it), the language at the right
    panel::Box viewLabel, viewSeg[3], langSeg[3];
    // 3.0.11-beta.174: the site's item now in a pill above the status - its title, its «Усі новини» link
    panel::Box news, newsText, newsLink;
};
// The strip's words that set its places (UTF-8; an empty one - its part left out)
struct Content {
    std::string status, viewLabel, news, allNews;
    std::string viewNames[3];  // 3.0.11-beta.177: the views' segments as wide as their words (96 at least)
};
Layout layout(int targetW, int targetH, float scale, const Content& c);
Layout layout(int targetW, int targetH, float scale, const std::string& statusText);  // the status alone
enum class Part : uint8_t { None, View0, View1, View2, Lang0, Lang1, Lang2, News, AllNews };
Part hitTest(const Layout& L, float x, float y);  // in the strip's pixels
constexpr const char* kLangNames[3] = {"УКР", "РУС", "ENG"};
struct Show {
    std::string status;  // UTF-8
    uint32_t statusRgb = 0x9AA3B5;
    int lang = 0;            // 3.0.11-beta.174: the chosen one of kLangNames
    bool views = false;      // the views' switch shown (3.0.11-beta.177+)
    int view = 1;            // the chosen view
    std::string viewLabel;   // «Вигляд»
    std::string viewNames[3];
    std::string news, allNews;  // 3.0.11-beta.174: the site's item now, its link's words
    int hover = 0;           // a Part's number, 0 none
};
enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xC9CFDB;
    float px = 14;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Show& sh);

}  // namespace uagfx::loginchrome
