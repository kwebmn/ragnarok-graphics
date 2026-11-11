#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::myshop {

// 3.0.11-beta.166 (task 4 of the remaining windows, the merchant's: one's own open shop): the game's own-shop window
// (UI window 0x2D, FUN_004b0a60) as a card in style A: the shop's sign, a row a thing still for sale (its icon, its
// name, its price, how many are left), what is left is worth, «Закрити лавку» the window's own close (6 / 0x87: the
// game mode's 0x5a closes the shop). The parts that need no Windows; the words by GDI (win/my_shop).
constexpr int kCloseButton = 0x87;
struct Row {
    std::string name;  // UTF-8
    int refine = 0, left = 0;
    long long price = 0;
    const panel::Picture* icon = nullptr;
    int index = 0;        // 3.0.18-beta.143: its place in the shop (the server's 0x0137 names it)
    std::string inside;   // its cards and free slots, a forged one's maker
    std::string colour = "#eef2f8";  // 3.0.18-beta.144: its name's (its element's)
    uint32_t info = 0;    // its ITEM_INFO in the game (its card)
};
long long worth(const std::vector<Row>& rows);

struct Texts {
    std::string title, left, worth, close, empty;  // left: «лишилось {1}»
};
struct Show {
    int scroll = 0;
    bool hoverClose = false;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, cross, list, sum, close;
    std::vector<panel::Box> rows;  // shown (8 at most)
};
Layout layout(int targetW, int targetH, float scale, size_t count);
int mostScroll(const Layout& L, size_t count);
enum class Part : uint8_t { None, Close, Body };
struct Hit {
    Part part = Part::None;
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
std::vector<Words> words(const Layout& L, const std::vector<Row>& rows, const Show& sh, const Texts& t);

// 3.0.18-beta.30 (uaUI's own shop): a row's icon square
panel::Box iconOf(const panel::Box& row, float s);

// 3.0.18-beta.143 (the windows' audit: «Моя лавка» showed nothing of what sold): the sales - the server's 0x0137 (a
// place, how many; no buyer) matched to the shop's things by their place (ITEM_INFO +8): each sale's name and zeny,
// how many of a place sold, the zeny earned since the shop opened; the newest sale first
struct Sale {
    std::string time, name;  // "10:42"; UTF-8
    int amount = 0;
    long long zeny = 0;
};
class Ledger {
public:
    void stock(int index, const std::string& name, long long price);  // a thing on sale (again: kept)
    bool sold(int index, int amount, const std::string& time, Sale& out);
    int soldOf(int index) const;
    long long earned() const { return earned_; }
    const std::vector<Sale>& sales() const { return sales_; }
    void clear();

private:
    struct Thing {
        std::string name;
        long long price = 0;
        int sold = 0;
    };
    std::map<int, Thing> things_;
    std::vector<Sale> sales_;
    long long earned_ = 0;
};

}  // namespace uagfx::myshop
