#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::buffpanel {

// 3.0.11-beta.35 (stage 7 of the new interface, the HUD - the buffs first; the owner approved «Баффы — столбиком:
// значок и время под ним, по кнопке столбик раскрывается в список с названиями», «горизонтальный ряд — на выбор в
// настройках»): the player's buffs at the screen's right edge under the minimap, style A (the concept's
// v19_vbuffs_ab, v20_layout) - each icon in a ring of its time left with the time under it, favourites first and
// larger, debuffs last in red rings and never hidden, the name in a tip over the hovered one; «назви» opens the list
// with the names; a row under the minimap the other look. The parts that need no Windows: their order, their places,
// their picture; the words go over it by GDI (win/buff_panel).

enum class Look : uint8_t { Column, Names, Row };
enum class Kind : uint8_t { Favourite, Buff, Debuff };

struct Entry {
    uint16_t icon = 0;                        // the game's status icon (the server's SI_*)
    std::string name;                         // its words (the game's, English)
    std::string time;                         // buffs::label - "45", "4:32", "29m"; "" not known
    float left = -1;                          // its time left of its whole (0..1), the ring; < 0 not known
    bool warn = false;                        // under 10 seconds left
    Kind kind = Kind::Buff;
    const panel::Picture* picture = nullptr;  // its icon at iconSide(kind), premultiplied; none: a dark tile
};

// The server's harmful states among the client's icons (src/map/status.h SI_*): Provoke, Quagmire, Signum Crucis,
// Decrease AGI, Lex Aeterna, broken armour and weapon, Hallucination, the weights, the strips, Stop, Bleeding, Joint
// Beat, Wink Charm, Close Confine on its target, Slow Cast, Critical Wounds
bool debuff(uint16_t icon);
// Words for an icon no skill or item of the server's gives (the weights, the broken gear ...); "" none
const char* stateName(uint16_t icon);
// The order shown: favourites, the rest, debuffs - each kind as the game has them
std::vector<int> order(const std::vector<Entry>& entries);

constexpr int kBig = 32, kSmall = 24;  // the icons' sides at scale 1: favourites, the rest and debuffs
int iconSide(Kind kind, float scale);   // the target's pixels

struct Slot {
    int entry = -1;
    panel::Box frame, icon, time, name;  // the ring round the icon, the icon, its time's line, its name's (Names)
};
struct Layout {
    float s = 1;
    Look look = Look::Column;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box body;                 // the glass under the icons
    panel::Box toggle;               // «‹ назви» / «сховати назви ›» over it; w 0: none (the row)
    std::vector<Slot> slots;         // the entries shown, in order
    std::vector<panel::Box> seps;    // the lines between the kinds
    panel::Box more;                 // «ще N»: the ones that did not fit; w 0: none
    int hidden = 0;
    panel::Box hit;                  // what takes the mouse: the toggle and the glass
    panel::Box tipRoom;              // where a tip may stand: left of the column, under the row
};
// The panel for `entries` with its top right corner at (right, top) in the target, reaching `bottom` at the most (the
// row: `widest` across at the most); its box sizes at `scale` (the interface's). The ones that do not fit go into
// «ще N» - the rest's last first, then the favourites'; debuffs never
Layout layout(const std::vector<Entry>& entries, Look look, int right, int top, int bottom, int widest, float scale);

enum class Part : uint8_t { None, Toggle, Icon, More };
struct Hit {
    Part part = Part::None;
    int slot = -1;
};
Hit hitTest(const Layout& L, float x, float y);  // the picture's pixels; None inside `hit` is still the panel's

// The tip over a slot: its box in the picture (the column: left of the glass, centred on the icon; the row: under
// the glass) for words `width` wide
panel::Box tipBox(const Layout& L, int slot, float width);
// The words' width as drawn at `px` (an estimate a little wide: the boxes made from it hold the GDI's words)
float wordsWidth(const std::string& utf8, float px, bool bold);

void paint(panel::Picture& out, const Layout& L, const std::vector<Entry>& entries, int hover);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
struct Texts {
    std::string names, hideNames;  // the toggle's: «назви», «сховати назви»
    std::string more;              // «ще {1}»
    std::string star, unstar;      // the tip's second line: «клік — в обрані», «клік — прибрати з обраних»
};
std::vector<Words> words(const Layout& L, const std::vector<Entry>& entries, int hover, const Texts& t);
// The hovered one's tip (the column and the row): its name and time, a favourite's or the rest's click; its ground
// here, its words in words(); w 0: none
panel::Box tipOf(const Layout& L, const std::vector<Entry>& entries, int hover, const Texts& t);
void paintTip(panel::Picture& out, const Layout& L, const std::vector<Entry>& entries, int hover, const Texts& t);

// An icon's picture at `side` x `side` (premultiplied, its corners rounded) from the game's .tga pixels (ARGB; a
// picture without alpha keys out its magenta, as the game's effects do)
panel::Picture iconPicture(const std::vector<uint32_t>& argb, int width, int height, bool alpha, int side,
                           float radius);

}  // namespace uagfx::buffpanel
