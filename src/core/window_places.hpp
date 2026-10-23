#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace uagfx::places {

// 3.0.3-beta.2 (the user, several game windows: each character's window at its own place):
// uaro-graphics\uaro-windows.txt - where each character's game window was put, a line "x y name" (the window's
// top-left corner in Windows' screen coordinates, the name to the line's end as the game spells it). ';' starts a
// comment; a line not like that is skipped; of two lines for a name the last counts.
struct Place {
    int x = 0, y = 0;
};
using Places = std::map<std::string, Place>;

Places parse(const std::string& text);
std::string format(const Places& places);

// What a game window's place needs now
struct Step {
    enum class Do { Nothing, Move, Keep } what = Do::Nothing;
    Place place;  // Move: where to; Keep: what to keep for the character
};
// A character's place on file (asked as it comes)
using Lookup = std::function<std::optional<Place>(const std::string& name)>;

// A game window's place as its characters come and the player moves it: a character that comes into the game gets
// its place (Move), a window the player moved and that stays a second there is kept for its character (Keep). Only a
// window the player can move counts; where such a window is when it becomes one is no move of the player's.
class Tracker {
public:
    static constexpr uint64_t kStayMs = 1000;
    // `name`: the character in the game ("" none); `now`: the window's top-left corner; `ms`: a clock
    Step step(const std::string& name, bool movable, Place now, uint64_t ms, const Lookup& lookup);
    void at(Place where);  // where a Move put the window (Windows may put it elsewhere)

private:
    std::string placed_;  // the character the window's place belongs to
    bool known_ = false;  // at_ holds where the window was put or last kept
    Place at_, moved_;
    uint64_t movedAt_ = 0;
};

}  // namespace uagfx::places
