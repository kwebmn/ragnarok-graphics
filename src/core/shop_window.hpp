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
// words by GDI (win/shop_window). 3.0.11-beta.101 (the owner: «чтобы в диалоге покупки работали также кнопки на
// клавиатуре верх вниз, также чтобы можно было легко вернуть выбраную на продажу вещь, чтобы если что можно было ввести
// количество с клавиатуры. продумай хорошо механизм», «и чтобы описание вещи можно было посмотреть»): a frame the keys
// and the controller move over the list and the cart, the amount typed into a row, its cross taking it back, a row's
// most by the zeny and the weight left, the keys under it all.
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

struct Data {
    Tab tab = Tab::Buy;
    std::string name, place;  // «Tool Dealer», «prontera»; the name "" not known
    std::vector<Ware> wares;
    std::vector<Line> cart;
    long long zeny = 0;
    int weight = 0, weightMost = 0;
    bool waiting = false;  // the other tab asked for, its list not here yet
};

// 3.0.11-beta.101 (the owner: «чтобы если что можно было ввести количество с клавиатуры»): the most a row may hold -
// sold: what the bag holds; bought: what the zeny and the weight left beside the cart's other rows allow (no more than
// kMostBought)
int mostFor(const Data& d, int ware);
// `by` more of the ware in the cart (fewer when negative), within 0 .. its most; at 0 its row leaves the cart
void put(Data& d, int ware, int by);
void takeBack(Data& d, int ware);  // 3.0.11-beta.101: its whole row out of the cart
int amountOf(const std::vector<Line>& cart, int ware);
std::vector<int> shownOf(const std::vector<Ware>& wares, const std::string& search);  // the names holding it, any case
std::string weightText(int tenths);  // «7», «0.1»
std::string money(long long zeny);   // «36 000 z»
int scrollFor(int focus, int scroll, int rows, int count);  // the scroll that keeps the row `focus` in sight

// 3.0.11-beta.101: an amount typed - the first digit replaces the ware's amount (its row made when it had none), the
// next ones follow, kept within its most (shown as the most then); the row stays while typed even at 0; the typing ends
// as typed (0: the row leaves) or back as it was
struct Edit {
    int ware = -1;  // -1: none typed
    std::string digits;
    int before = 0;  // its amount when the typing began
};
void typeDigit(Data& d, Edit& e, int ware, char digit);
void eraseDigit(Data& d, Edit& e);
void endEdit(Data& d, Edit& e);
void cancelEdit(Data& d, Edit& e);

// 3.0.11-beta.101: a held button's steps (the controller's A and X) - one at the press, after 0.4 s one every 80 ms,
// after 1.5 s ten at a time
class HoldRepeat {
public:
    int step(bool held, uint32_t nowMs);

private:
    bool held_ = false;
    uint32_t downAt_ = 0, lastAt_ = 0;
};

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

struct Texts {
    std::string title;  // «Крамниця»: the NPC's name not known
    std::string buy, sell, search, ware, price, weight, cart, amount, sum;
    std::string total, left, after, weightBuy, weightSell;  // «Разом», «Залишиться», «Буде», «Вага після покупки» ...
    std::string clear, note, empty, waiting;
    // 3.0.11-beta.101: the keys' and the controller's words in the footer: «вибір», «список / кошик», «+1», «−1»,
    // «кількість», «прибрати», «опис», «Закрити», the space bar's name; the typing's hint («... найбільше {1}»)
    std::string choose, toCart, plus, minus, typeAmount, remove, info, close, space;
    std::string editHint;
};

using Measure = std::function<float(const std::string& utf8, float px, bool bold)>;

struct Show {
    int hoverTab = -1, hoverWare = -1, hoverLine = -1;  // hoverWare: a place in the shown list
    int hoverLess = -1, hoverMore = -1, hoverAmount = -1, hoverTakeBack = -1;
    bool hoverConfirm = false, hoverClear = false, hoverClose = false, hoverSearch = false;
    int scroll = 0, cartScroll = 0;
    bool typing = false, caretOn = false;  // the search has the keys
    std::string search;
    bool pad = false, sony = false;
    // 3.0.11-beta.101: the frame the keys and the controller move (a place in the shown list, or a row of the cart),
    // shown once they moved it or a row was clicked; the amount typed
    bool cartFocus = false, framed = false;
    int ware = 0, line = 0;
    Edit edit;
};

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, tabsRow, searchBox, listHead, list, cartTitle, cartHead, cart, totals;
    panel::Box confirm, confirmKey, clear, note, footer;  // confirmKey: «Enter» or the controller's Start on it
    std::vector<panel::Box> tabs;  // «Купити», «Продати»
    std::vector<panel::Box> wares;  // the list's rows on the screen: shown[firstWare] ...
    // the cart's rows on the screen (cart[firstLine] ...): − the amount + and the take-back cross
    std::vector<panel::Box> lines, less, amounts, more, drops;
    int firstWare = 0, firstLine = 0;
    panel::Box tabGlyphs[2];  // LB and RB beside the tabs while played with the controller
    std::vector<panel::Box> padGlyphs, padWords;
    std::vector<panel::Box> keyCaps, keyWords;  // 3.0.11-beta.101: the keys and their words, without the controller
};
Layout layout(int x, int y, float scale, const Data& d, const std::vector<int>& shown, const Show& sh,
              const Texts& t, const Measure& m);
int iconSide(float scale);

enum class Part : uint8_t {
    None, Close, Tab, Search, Ware, Line, Less, Amount, More, TakeBack, Confirm, Clear, List, Cart, Title, Body
};
struct Hit {
    Part part = Part::None;
    int index = -1;  // Ware: the place in the shown list; Line ... TakeBack: the cart's row; Tab: 0 buy, 1 sell
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
