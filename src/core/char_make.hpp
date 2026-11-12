#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::charmake {

// 3.0.11-beta.153 (task 3 of the remaining windows, the login's part three): the game's character make (UI window 4)
// in style A at the screen's middle - the face with the hair's style and colour (‹ ›) on the left; the name (the
// game's own field) and the six stats in the game's three pairs on the right (a stat's + takes a point from its pair,
// each pair 10 together - FUN_0045c5f0); «Створити», «Скасувати» press its own buttons. The parts that need no
// Windows; the words by GDI (win/char_make).
struct Fields {
    std::string name;  // UTF-8, the game's field's
    bool focus = false, caret = false;
    int stats[6] = {5, 5, 5, 5, 5, 5};  // STR AGI VIT INT DEX LUK
    int hair = 1, colour = 0;
    const panel::Picture* face = nullptr;  // its head at faceSide(scale), premultiplied
};
int faceSide(float scale);

struct Pair {
    int left, right;  // the stats' places in Fields::stats
};
constexpr Pair kPairs[3] = {{0, 3}, {1, 5}, {2, 4}};  // STR - INT, AGI - LUK, VIT - DEX
// The game's buttons: a point to the pair's left stat (from its right one) or to its right one
int pointButton(int pair, bool toLeft);  // 0x8b STR, 0x8e INT, 0x8c AGI, 0x90 LUK, 0x8d VIT, 0x8f DEX
bool canMove(const Fields& f, int pair, bool toLeft);  // the giving stat keeps 1 at least
int hairButton(bool after);                            // 0xa0 the style after (1-23, round), 0xa1 before
constexpr int kColourButton = 0xd5;                    // the colour on (0-8, round)
int colourPresses(bool after);                         // 1, or 8 for the one before
constexpr int kMakeButton = 0x76, kCancelButton = 0x77;

struct Texts {
    std::string title, name, nameHint, hair, colour, pairsHint, make, cancel;
    std::string keysHint;  // 3.0.18-beta.228: «↑ ↓ рядок · ← → змінити»
    std::string stats[6];
};
enum class Part : uint8_t { None, Name, HairBefore, HairAfter, ColourBefore, ColourAfter, PairLeft, PairRight, Make,
                            Cancel };
struct Show {
    Part hover = Part::None;
    int hoverIndex = -1;
    int focusRow = -1;  // the controller's row: 0 the hair, 1 its colour, 2-4 the pairs
};
constexpr int kRows = 5;  // the controller's: the hair, its colour, the three pairs
// 3.0.18-beta.228 (the keyboard concept, stage 5): the rows by the arrows - ↑ ↓ round the name (-1: its field's caret
// takes ← →) and the kRows; ← → change a row (not the name's)
int keyRow(int row, bool down);
bool keySides(int row);
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, face, hairLabel, hairBefore, hairValue, hairAfter, colourLabel, colourBefore,
        colourValue, colourAfter, nameLabel, name, pairsHint, make, cancel;
    panel::Box pairs[3], pairLeft[3], pairRight[3], pairBar[3], leftWords[3], rightWords[3];
};
Layout layout(int targetW, int targetH, float scale);
struct Hit {
    Part part = Part::None;
    int index = -1;
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
}  // namespace uagfx::charmake
