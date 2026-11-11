#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::vendsetup {

// 3.0.16-beta.33 (the owner: «делай» - the shop's opening in the new style): the game's two vending windows - the
// cart's things one may put up (UI window 0x2A, FUN_004b8760) and the shop being made (0x29, FUN_004bfe40: its sign,
// a price a place) - as one card in style A: «Назва лавки», «Візок» (a click puts a thing up), «На продаж n / most» (a
// price field each, ✕ takes it back), «Відкрити лавку» (the game's own OK: its checks of the prices, its questions),
// «Скасувати». The fields are the game's own (their words typed into them as the game types). The parts that need no
// Windows; the words by GDI (win/vend_setup).
struct Row {
    std::string name;  // UTF-8
    int count = 0, refine = 0;
    int id = 0;  // 3.0.18-beta.104: the item's number (its market line)
    const panel::Picture* icon = nullptr;
    std::string price;  // the sale's: its field's words (digits)
    std::string inside;  // 3.0.18-beta.142: its cards and free slots (win/info_card describe), "" none
    std::string colour = "#eef2f8";  // 3.0.18-beta.144: its name's (its element's)
};
// A price as the game takes it (FUN_0062a7b0 then the OK's checks): its digits, at most 99 990 000; 0 none
long long priceOf(const std::string& words);
// 3.0.16-beta.39 (the pad: LB / RB on a sale's row): the price one step down (dir < 0) or up - a power of ten, 1-10 % of
// it (1 under 100; up from 0 to 10), rounded to the step, 0 .. 99 990 000; its digits
std::string stepPrice(const std::string& words, int dir);

struct Texts {
    std::string title, sign, signHint, cart, sale, open, cancel, empty, priceHint;
    std::string hint;  // 3.0.16-beta.39: the controller's buttons under the card's own, "" none  // sale: «На продаж {1} / {2}»
};
enum class Field : int8_t { None = -1, Sign = 0 };  // a price's field: 1 + its place
struct Show {
    int cartScroll = 0, saleScroll = 0;
    int hoverCart = -1, hoverSale = -1, hoverRemove = -1, hoverButton = -1;  // the button: 0 open, 1 cancel, 2 ✕
    int focus = -1;     // Field: the sign 0, a price 1 + its place, -1 none
    bool caret = true;  // the blink's on half
    int most = 0;       // the places for sale (the skill's level + 2)
    int padCart = -1, padSale = -1;  // 3.0.16-beta.39: the controller's row on screen (a gold edge), -1 none
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, cross, signLabel, sign, cartHead, saleHead, cartList, saleList, open, cancel;
    panel::Box hint;  // 3.0.16-beta.39: h 0 without it
    std::vector<panel::Box> cartRows, saleRows, prices, removes;  // shown
};
Layout layout(int targetW, int targetH, float scale, bool hint = false);
int mostScroll(const Layout& L, bool sale, size_t count);
enum class Part : uint8_t { None, Body, Cross, Open, Cancel, Sign, Cart, Sale, Price, Remove };
struct Hit {
    Part part = Part::None;
    int index = -1;  // a row's place on screen (the scroll added by the caller)
};
Hit hitTest(const Layout& L, float x, float y);


enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
// 3.0.18-beta.30 (uaUI's shop opening): a row's icon square
panel::Box iconOf(const panel::Box& row, float s);

}  // namespace uagfx::vendsetup
