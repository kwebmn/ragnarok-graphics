#pragma once
#include <string>

namespace uagfx::map_load {

// 3.0.2-beta.12 (the client build session: prt_fild08 loads in 3.7 s, cps.dll's inflating 6 times as fast changed
// nothing): a map's loading as the driver's frames see it - from the first frame without the world under a map's
// name (uaRO.exe copies the name into its game mode and shows the loading picture as the map's start begins,
// 0x559030) to the first frame with the world again. A world unseen for 300 ms or less in the same map is no load; a
// load that never ends (two minutes, or back to the character list) is dropped.
class Watch {
public:
    enum class Event { None, Began, Ended, Dropped };
    // A frame shown at `ms` (one clock, milliseconds): the game's map ("" at the login and the character list) and
    // whether its world is in the frame
    Event frame(double ms, const std::string& map, bool world);
    bool loading() const { return loading_; }
    const std::string& map() const { return map_; }  // the map being loaded
    double loadMs() const { return loadMs_; }         // at Ended: from its loading picture to its world
    double beforeMs() const { return beforeMs_; }     // the old map's last frame to the loading picture; < 0: none

private:
    bool loading_ = false;
    bool blocked_ = false;    // dropped for its length: none begins until a world is seen
    bool hadWorld_ = false;
    double worldMs_ = 0.0;    // the last frame with a world
    std::string worldMap_;    // its map
    double began_ = 0.0;
    std::string map_;
    double loadMs_ = 0.0;
    double beforeMs_ = -1.0;
};

}  // namespace uagfx::map_load
