#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::shelftoast {

// 3.0.11-beta.175 (the login concept's shelf): «kweb додано на полицю · Скасувати» - a pill at the screen's top
// middle for kShowMs after a login was put on the shelf by itself; «Скасувати» takes it off again. The parts that
// need no Windows; the words by GDI (win/login_card).
constexpr int kShowMs = 6000;
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the pill in the target's pixels
    panel::Box pill, text, undo;     // in the pill's picture
};
Layout layout(int targetW, int targetH, float scale, const std::string& text, const std::string& undo);
bool overUndo(const Layout& L, float x, float y);  // in the pill's pixels
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 13.5f;
    bool bold = false;
};
std::vector<Words> words(const Layout& L, const std::string& text, const std::string& undo, bool hot);

}  // namespace uagfx::shelftoast
