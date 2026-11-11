#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::vendshop {

// 3.0.11-beta.156 (task 4 of the remaining windows, the merchant's: buying from another's shop): the game's two shop
// windows (its things - UI window 0x2B, the purchase - 0x2C) as one card «Лавка» in style A: a row a thing (its icon,
// its name, its price, how many the shop has), the purchase chosen in the same rows (− and + beside the count - the
// game's own purchase list), the total against the zeny, «Купити» and «Скасувати» its own buttons. The parts that need
// no Windows; the words by GDI (win/vend_shop).
struct Row {
    int index = 0, id = 0, refine = 0;
    long long price = 0;
    int stock = 0;   // how many the shop has
    int chosen = 0;  // how many of it the purchase has
    std::string name;  // UTF-8
    std::string inside;  // 3.0.18-beta.138: its cards and free slots («Hydra Card · 1 вільний»), "" none
    std::string colour = "#eef2f8";  // 3.0.18-beta.144: its name's - its element's, else the plain one
    const panel::Picture* icon = nullptr;
};
// 3.0.18-beta.138 (the owner: things in a shop not seen into): the slots after the name as the game's
// own windows write them («Gladius [3]», none when 0); what is inside: each card's name, then the free slots by
// `freeWord` («{1} вільних»); a forged or named thing's makers (255, 254, 0xFF00 first) are no cards
std::string withSlots(const std::string& name, int slots);
std::string inside(const int cards[4], int slots, const std::function<std::string(int)>& cardName,
                   const std::string& freeWord);
long long total(const std::vector<Row>& rows);                   // each chosen times its price
bool canAdd(const Row& r, long long purchase, long long zeny);  // the shop has more, the zeny pays for one more
bool canRemove(const Row& r);
std::string money(long long zeny);  // "1 500 000"
constexpr uint32_t kShort = 0xFF6B5E;  // the total's colour when the zeny does not pay for it

struct Texts {
    std::string title;           // the shop's sign («Лавка» when not known)
    std::string stock;           // «є {1}»
    std::string total, zeny;     // «Разом», «Ваші зені»
    std::string buy, cancel;     // «Купити», «Скасувати»
    std::string hint;            // the controller's buttons («A: +1 · X: −1 · Y: купити»), "" none
};
struct Show {
    int scroll = 0;
    int hoverRow = -1;   // of all (the scroll counted)
    int hoverPart = 0;   // its Part
    int focus = -1;      // the controller's row, -1 none
    long long zeny = 0;  // the character's
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, list, sums, total, zeny, hint, buy, cancel;
    std::vector<panel::Box> lines, minus, counts, plus;  // the rows shown (8 at most)
};
Layout layout(int targetW, int targetH, float scale, size_t rows);
int mostScroll(const Layout& L, size_t rows);
enum class Part : uint8_t { None, Row, Minus, Plus, Buy, Cancel, Close, Title };
struct Hit {
    Part part = Part::None;
    int row = -1;  // of all (the scroll counted)
};
Hit hitTest(const Layout& L, int scroll, size_t rows, float x, float y);


enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
// 3.0.18-beta.30 (uaUI's «Лавка»): a line's icon square
panel::Box iconAt(const Layout& L, size_t line);

}  // namespace uagfx::vendshop
