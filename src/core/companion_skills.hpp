#pragma once
#include "core/window_panel.hpp"

namespace uagfx::companion {

// 3.0.16-beta.31 (the owner: «делай» - the homunculus' and the mercenary's skills in the new style): what the game's
// own skills window (UI windows 0x72 / 0x85, class 0x69B9D0 in its kinds 1 and 2, FUN_004bdd90) did for them and the
// cards (core/homun_card, core/merc_card) did not - the level a skill is used at, chosen (‹ ›), and a skill dragged
// onto the hotbar. The parts that need no Windows.

// The level a skill is used at: the game's stored one while it is 1..level, else the skill's level (as the window's
// refresh puts it back)
int useOf(int stored, int level);
int stepped(int use, int level, int by);  // one step down or up, within 1..level

// A skill's row's parts: the use level's ‹ and › to the left of its words (a free place for the raise «+» at the row's
// right end when `raise`)
struct RowParts {
    panel::Box less, more, level;  // level: the words between them
};
RowParts rowParts(const panel::Box& row, float s, bool raise);

// A press on a row moved this far (target pixels) is a drag
bool dragged(float x0, float y0, float x, float y, float s);

}  // namespace uagfx::companion
