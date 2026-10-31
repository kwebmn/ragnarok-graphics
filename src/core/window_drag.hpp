#pragma once
#include <cstdint>

namespace uagfx::windrag {

// 3.0.11-beta.110 (the concept «Концепция окон и инвентаря», stage 2): the windows' drags as one. A press on a thing
// becomes the window's own drag once the cursor moved past 10 pixels times the scale (till let go); released() - the
// thing a drag let go (the window begins the game's drag of it and lets it go there), -1 after a click or nothing
// pressed (beta.106's skills' press, now «Речі»'s, «Навички»'s and «Спорядження»'s)
struct Press {
    int index = -1;  // the thing pressed (the window's own number of it), -1 none
    float x = 0, y = 0;
    bool dragging = false;
};
void pressed(Press& p, int index, float x, float y);
bool moved(Press& p, float x, float y, float scale);  // dragging now
int released(Press& p);

// What is dragged now, told to the windows that take it: the slots it fits in «Спорядження», the hotbar's cells, the
// bag for a worn thing - lit in gold. On the game's thread
enum class Kind : uint8_t { None, Item, Worn, Skill };
struct Thing {
    Kind kind = Kind::None;
    int id = 0;
    uint32_t location = 0;  // where a thing may be worn (an item's) or is worn (a worn one's)
    int type = 0;
};
void set(const Thing& t);
void clear();
const Thing& now();

}  // namespace uagfx::windrag
