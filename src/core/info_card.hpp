#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/npc_talk.hpp"
#include "core/window_panel.hpp"

namespace uagfx::infocard {

// 3.0.11-beta.149 (the owner: «сделай все без остановки, в инлайн режиме» - the game's windows still in the old look,
// the item's and the skill's cards first): the game's item card (UI window 0xC) and skill card (0x2E) in style A - the
// icon and the name in the title, the game's description from its own table under them (its ^RRGGBB colours made to
// read on the dark glass, its white spacers hidden, its line breaks kept), an item's cards in its slots, an item's big
// picture beside the words when the client has one; the wheel scrolls a long one. The parts that need no Windows; the
// words by GDI (win/info_card).

// The game's description tables: «1201#», its lines, «#» (items); «MG_FIREWALL#» alike (skills) - a key to its lines;
// the «//» lines and the CRs left out
std::map<std::string, std::vector<std::string>> descriptions(const std::string& text);
// itemslotcounttable.txt: «1101#3#» - an item's number to its slots
std::map<int, int> slotCounts(const std::string& text);
// The folder of data\ the game reads its language's tables from, by Windows' language (uaRO.exe 0x4EF2xx, its primary
// id): «german\», «italian\», «portuguese\», «turkish\», else «english\» (Ukrainian and Russian among them)
std::string languageFolder(unsigned langId);

struct Card {
    std::string title;               // «+7 Blade [4]», «Fire Wall»
    std::string note;                // under it: «×38», «Lv 10», «Не опізнано»; "" none
    std::vector<std::string> lines;  // the description, UTF-8 with the game's ^RRGGBB
    std::vector<std::string> cards;  // an item's slots: a card's name, "" an empty slot
    const panel::Picture* icon = nullptr;     // at iconSide(scale), premultiplied
    const panel::Picture* picture = nullptr;  // the big picture, fitted into pictureRoom (premultiplied)
};
int iconSide(float scale);
void pictureRoom(float scale, int& w, int& h);
// An image (straight alpha) fitted into w x h with its sides in proportion, premultiplied; none: an empty picture
panel::Picture fitted(const std::vector<uint32_t>& argb, int width, int height, int w, int h);

struct Texts {
    std::string slots;  // «Слоти»
    std::string empty;  // «порожньо»
};
struct Show {
    int scroll = 0;  // the first row shown
    bool hoverClose = false;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box glass, title, note, close, icon, picture, body, slotsLabel;
    std::vector<npctalk::Placed> lines;  // every row's runs, their y as if not scrolled
    int rows = 0, shownRows = 0;
    float lineH = 0;
    std::vector<panel::Box> slots;
};
// Its top left at (x, y) of the target, kept on its targetW x targetH
Layout layout(const Card& c, int x, int y, int targetW, int targetH, float scale, const Show& sh, const Texts& t,
              const npctalk::Measure& m);
enum class Part : uint8_t { None, Close, Body, Title };
Part hitTest(const Layout& L, float x, float y);  // the picture's pixels
int scrolled(const Layout& L, int scroll, int notches);  // the wheel's: three rows a notch (down positive), kept within

void paint(panel::Picture& out, const Layout& L, const Card& c, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Card& c, const Show& sh, const Texts& t);

}  // namespace uagfx::infocard
