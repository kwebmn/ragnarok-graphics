#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::popupcard {

// 3.0.11-beta.211 (the tester: «меню взаємодії з іншим гравцем - в старому стилі»): the game's popup menu (UI window
// 0x12 - a player's in the world, a chat room member's, a guild member's) as a card in style A at the click: its
// items as the game keeps them (each pushed twice - a half row each - a separator once), a click giving its owner the
// number the game's own click gives (the item's among the items). The parts that need no Windows; the words by GDI
// (win/popup_window).
struct Item {
    std::string text;  // UTF-8
    bool separator = false;
};
std::vector<Item> items(const std::vector<std::string>& raw, const std::string& separator);
int pickNumber(const std::vector<Item>& items, int row);  // -1: a separator or none

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass;
    std::vector<panel::Box> rows;
};
// At (atX, atY) of the target, inside it; `widest` the widest item's words at this scale
Layout layout(const std::vector<Item>& items, int atX, int atY, int targetW, int targetH, float scale, float widest);
int rowAt(const Layout& L, const std::vector<Item>& items, float x, float y);  // an item's row (not a separator); -1

void paint(panel::Picture& out, const Layout& L, const std::vector<Item>& items, int hover);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const std::vector<Item>& items, int hover);

}  // namespace uagfx::popupcard
