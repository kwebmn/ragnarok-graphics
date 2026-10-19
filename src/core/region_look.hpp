#pragma once
#include <string>

namespace uagfx::region_look {

// 0.79: each region its own colours (effects.region_looks) - Payon warm and green, Niflheim cold and pale, Morroc
// sun-bleached, Lutie blue and clean, Glast Heim grim... by the map's name; a map of no known region keeps the
// game's. The tint multiplies the colours, saturation 1 is the game's.
struct Look {
    float tint[3] = {1.0f, 1.0f, 1.0f};
    float saturation = 1.0f;
    const char* region = "";  // for the log; "" none
};

// strength 0-100: how much of the region's look (0 none, 100 all of it).
Look of(const std::string& map, int strength);

// 2.5.21: the look makes the picture other than the game's (a tint or a saturation of its own) - a region's at
// strength 0 does not
bool changes(const Look& look);

}  // namespace uagfx::region_look
