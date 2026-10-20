#pragma once
#include <string>

namespace uagfx::living {

// 1.7, the living world: what a map is like for its small details - by its name, as region_look and weather go.
struct Place {
    bool sand = false;        // desert ground: the wind drifts sand over it (sand_drift)
    bool snow = false;        // snow on the ground: it glitters (snow_sparkle), breath shows (breath)
    bool cave = false;        // a damp cave or dungeon: its walls glisten (wet_walls), water drips (cave_drips)
    bool underwater = false;  // under the sea: bubbles and light from above (underwater)
    bool swamp = false;       // stagnant water: bubbles and a low mist (swamp)
    int flowers = -1;         // the grass's flowers, their colours: 0 a meadow's, 1 Payon's, 2 Amatsu's, 3 the
                              // tropics'; -1 none
    bool wildlife = false;    // butterflies and birds by day (fields and green towns)
    int leaves = 0;           // what falls from the crowns (leaf_fall): 0 green leaves, 1 autumn's, 2 sakura petals
    bool operator==(const Place& o) const {  // 2.7.5: the same place keeps its creatures (a season of the same leaves)
        return sand == o.sand && snow == o.snow && cave == o.cave && underwater == o.underwater && swamp == o.swamp &&
               flowers == o.flowers && wildlife == o.wildlife && leaves == o.leaves;
    }
};

Place placeOf(const std::string& map, bool outdoors, bool autumn);

// The minute of the day now, by the computer's clock (0-1439).
int localMinute();
// The morning's low fog (morning_fog), 0-1: from 4:30 up, thick 6:00-8:00, gone by 10:30.
float morningFog(int minute);
// Dew on the grass (dew), 0-1: at dawn and in the morning, gone by noon.
float dew(int minute);
// Daytime for the creatures (butterflies, birds), 0-1: 7:00-19:00 with an hour to come and go.
float daytime(int minute);

}  // namespace uagfx::living
