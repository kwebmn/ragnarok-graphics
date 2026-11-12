#pragma once
#include <cstdint>

namespace uagfx::keywalk {

// 3.0.18-beta.207 (the keyboard concept https://claude.ai/artifact/SxvWxL5BM2tHtSWAvYPh9J, stage 1; the testers:
// «кнопками с клавиатуры верх вниз влево вправо управлять»): the arrows walk while the chat line is empty. The held
// arrows are the pad's left stick for its walk without the mouse (pad/core/logic/stick_walk): up is away from the
// camera, two arrows a diagonal; let go - the character stands at the next cell.

enum Dir : uint8_t { kUp = 1, kDown = 2, kLeft = 4, kRight = 8 };

uint8_t dirOfVk(unsigned vk);  // VK_UP 0x26, VK_DOWN 0x28, VK_LEFT 0x25, VK_RIGHT 0x27; 0 for any other key

struct Vec {
    float x = 0, y = 0;  // x right, y up - as the pad's stick
};
// The walk the held arrows ask: a diagonal as long as one arrow (1/sqrt 2 each way), opposite arrows cancel
Vec vectorOf(uint8_t held);

// When an arrow walks: «Ходьба стрілками» on, a map in sight (not the login or the character select), the chat line
// empty (words in it: the arrows move its caret), no Ctrl, Alt or Shift held
struct Gate {
    bool enabled = false, inGame = false, chatEmpty = false, modifier = false;
    bool otherField = false;  // another of the game's fields has the focus (a count, a name): its caret's arrows
};
bool arrowWalks(const Gate& g);

// 3.0.18-beta.210 (stage 4), moved here 3.0.18-beta.214: the pad button a key stands for in the driver's windows
// (win/key_nav): the arrows the D-pad, Enter A, Esc B, PgUp / PgDn LB / RB, the menu key or Shift+F10 Y; 0 none.
// Space none since .214 (R-CX44S5): as X it took a thing in «Речі» up to the bar, a carry for the pad alone
uint16_t windowButtonOf(unsigned vk, bool shift);

// 3.0.18-beta.215 (R-8KPDY9 «клавиатура тянула все время в право»): a key's let go can be taken by a hook earlier in
// the game window's chain - the walk mode switched on with an arrow held took the arrow's let go, and the arrows' walk
// went on for 30 s. Each frame the keys held are checked against Windows' own state of the keys (`down`: GetKeyState
// on the game's thread): a key no longer down lets go, whoever took its message
using KeyDown = bool (*)(unsigned vk);
uint8_t heldNow(uint8_t dirs, KeyDown down);              // the arrows (Dir bits) whose keys are still down
uint16_t windowButtonsNow(uint16_t buttons, KeyDown down);  // windowButtonOf's buttons whose keys are still down

}  // namespace uagfx::keywalk
