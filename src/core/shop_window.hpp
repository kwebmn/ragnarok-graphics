#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::refindex {
struct Shop;
}

namespace uagfx::shopwin {

// 3.0.11-beta.100 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's
// «Магазин»): the NPC's shop in style A in place of the game's deal windows - the NPC's name and the map, the tabs
// «Купити» / «Продати», the shop's list on the left (an icon, the name, the price, the weight) under a search, the cart
// on the right (− n + and each row's sum), the total, the zeny left (or after a sale), the weight after, «Купити ·
// Enter» and «Очистити»; under them what the mouse or the controller's buttons do. The parts that need no Windows; the
// words by GDI (win/shop_window).
enum class Tab : uint8_t { Buy, Sell };
constexpr int kMostBought = 30000;  // a cart row's most when bought (the purchase packet's amount is a short)
constexpr int kListRows = 10, kCartRows = 6;

struct Ware {  // a row of the shop's list as the game keeps it (the session's deal list)
    int id = 0;
    int price = 0;    // one's price (bought) or what one brings (sold): the server's, after Discount / Overcharge
    int weight = -1;  // in tenths (the server's item base); -1 not known
    int count = 0;    // sold: how many the bag holds, a row's most; bought: 0
    int refine = 0;
    bool identified = true;
    std::string name;
    const panel::Picture* icon = nullptr;
};
struct Line {  // a row of the cart
    int ware = 0;  // its place in the list
    int amount = 0;
};

int most(const Ware& w, Tab tab);
// `by` more of the ware in the cart (fewer when negative), within 0 .. its most; at 0 its row leaves the cart
void put(std::vector<Line>& cart, const std::vector<Ware>& wares, int ware, int by, Tab tab);
int amountOf(const std::vector<Line>& cart, int ware);
std::vector<int> shownOf(const std::vector<Ware>& wares, const std::string& search);  // the names holding it, any case
std::string weightText(int tenths);  // «7», «0.1»
std::string money(long long zeny);   // «36 000 z»
int scrollFor(int focus, int scroll, int rows, int count);  // the scroll that keeps the row `focus` in sight

struct Sums {
    long long total = 0;
    long long zenyAfter = 0;    // bought: what is left; sold: what there will be
    long long weightAfter = 0;  // the game's units
    int weightPercent = 0;
    bool ok = false;  // the confirm may go: something in the cart, money enough and not past the weight to buy it
};
Sums sums(Tab tab, const std::vector<Line>& cart, const std::vector<Ware>& wares, long long zeny, int weight,
          int weightMost);

// Which shop of the reference pack on `map` the NPC is: the most of `items` (the list it sells) among its wares, else
// (a sale's list, or none of them) the nearest to the character's cell within 15 cells; -1 none
int matchShop(const std::vector<refindex::Shop>& shops, const std::string& map, const std::vector<int>& items, int x,
              int y);

struct Data {
    Tab tab = Tab::Buy;
    std::string name, place;  // «Tool Dealer», «prontera»; the name "" not known
    std::vector<Ware> wares;
    std::vector<Line> cart;
    long long zeny = 0;
    int weight = 0, weightMost = 0;
    bool waiting = false;  // the other tab asked for, its list not here yet
};

struct Texts {
    std::string title;  // «Крамниця»: the NPC's name not known
    std::string buy, sell, search, ware, price, weight, cart, amount, sum;
    std::string total, left, after, weightBuy, weightSell;  // «Разом», «Залишиться», «Буде», «Вага після покупки» ...
    std::string buyButton, sellButton, clear, note, empty, mouse, waiting;
    std::string plus, minus, close;  // the controller's
};

using Measure = std::function<float(const std::string& utf8, float px, bool bold)>;

struct Show {
    int hoverTab = -1, hoverWare = -1, hoverLine = -1;  // hoverWare: a place in the shown list
    int hoverLess = -1, hoverMore = -1;
    bool hoverConfirm = false, hoverClear = false, hoverClose = false, hoverSearch = false;
    int scroll = 0, cartScroll = 0;
    bool typing = false, caretOn = false;  // the search has the keys
    std::string search;
    bool pad = false, sony = false;
    bool padCart = false;  // the controller's frame in the cart, else in the list
    int padWare = 0, padLine = 0;
};

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, tabsRow, searchBox, listHead, list, cartTitle, cartHead, cart, totals;
    panel::Box confirm, clear, note, footer;
    std::vector<panel::Box> tabs;               // «Купити», «Продати»
    std::vector<panel::Box> wares;              // the list's rows on the screen: shown[firstWare] ...
    std::vector<panel::Box> lines, less, more;  // the cart's rows on the screen (cart[firstLine] ...), their − and +
    int firstWare = 0, firstLine = 0;
    panel::Box tabGlyphs[2];                    // LB and RB beside the tabs while played with the controller
    std::vector<panel::Box> padGlyphs, padWords;
};
Layout layout(int x, int y, float scale, const Data& d, const std::vector<int>& shown, const Show& sh,
              const Texts& t, const Measure& m);
int iconSide(float scale);

enum class Part : uint8_t { None, Close, Tab, Search, Ware, Line, Less, More, Confirm, Clear, List, Cart, Title, Body };
struct Hit {
    Part part = Part::None;
    int index = -1;  // Ware: the place in the shown list; Line, Less, More: the cart's row; Tab: 0 buy, 1 sell
};
Hit hitTest(const Layout& L, float x, float y);

void paint(panel::Picture& out, const Layout& L, const Data& d, const std::vector<int>& shown, const Show& sh,
           const Measure& m);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Data& d, const std::vector<int>& shown, const Show& sh,
                         const Texts& t, const Measure& m);

}  // namespace uagfx::shopwin
