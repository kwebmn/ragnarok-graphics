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

}  // namespace uagfx::keywalk
