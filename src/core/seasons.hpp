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

// 2.7.5: the place as its year has it - no butterflies, dragonflies nor fireflies while snow lies (the birds stay);
// 2.7.6: no flowers in the grass either
living::Place withYear(living::Place place, const Look& year);

// 3.0.22-beta.24 (the owner 19.11 «делай 1-4», 3 - the city whitening in sight for the channel's videos): the year
// changing on the same map (the «Пора року» chosen anew, a new month) shown little by little - each of snow, dry and
// lush goes toward its new value by a whole step in fadeSeconds (0: at once); a new map takes its year at once
Look ease(const Look& shown, const Look& target, float seconds, float fadeSeconds);
bool same(const Look& a, const Look& b);

}  // namespace uagfx::seasons
