#pragma once
#include <string>

#include "core/living.hpp"

namespace uagfx::seasons {

// 1.20, the year on the whole map (effects.seasons; ground_life_ps): by the computer's calendar - winter's snow on
// what faces up (roofs, rocks' tops, crowns, the ground), autumn's drier grass, spring's greener ground. The snowy
// maps (Lutie and the like) keep snow on their roofs all year; the desert, the jungle, the tropics, caves and the sea
// have none of it. Each 0-1. 2.4: autumn's leaves on the ground are gone - specks a few pixels across over every
// paved square.
struct Look {
    float snow = 0.0f;
    float dry = 0.0f;   // the green ground towards straw
    float lush = 0.0f;  // the green ground greener
};

Look of(const std::string& map, int month, bool outdoors, const living::Place& place);

// 2.6.22, [effects] season (by the Clock): the year all that goes by it takes - the computer's calendar, or a season
// of the player's choice that stays, at the month in its middle (in the order of the file's words).
enum Choice { kSeasonAuto, kSeasonSpring, kSeasonSummer, kSeasonAutumn, kSeasonWinter, kSeasons };
int monthFor(int choice, int localMonth);  // localMonth: the computer's, 1-12

}  // namespace uagfx::seasons
