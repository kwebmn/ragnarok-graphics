#pragma once
#include <string>

namespace uagfx::map_load {

// 3.0.2-beta.12 (the client build session: prt_fild08 loads in 3.7 s, cps.dll's inflating 6 times as fast changed
// nothing): a map's loading as the driver's frames see it - from the first frame without the world under a map's
// name (uaRO.exe copies the name into its game mode and shows the loading picture as the map's start begins,
// 0x559030) to the first frame with the world again. A world unseen for 300 ms or less in the same map is no load
// (3.0.2-beta.14: unless the game showed its loading picture meanwhile - maps load in 0.25 s now); a load that never
// ends (two minutes, or back to the character list) is dropped. 3.0.2-beta.15 (beta.13 and 14: some loads slow again
// and unmeasured): uaRO.exe's new camera (0x4fa2e0) keeps its matrix's memory as it finds it - the old camera's when
// it gets the same block - so a loading picture could pass for the world; while the game's map start runs (0x559030,
// hooked) no frame is the world, and such a window is always a load.
class Watch {
public:
    enum class Event { None, Began, Ended, Dropped };
    // A frame shown at `ms` (one clock, milliseconds): the game's map ("" at the login and the character list),
    // whether its world is in the frame, (3.0.2-beta.14) whether it is the game's loading picture, (3.0.2-beta.15)
    // whether the game's map start is running
    Event frame(double ms, const std::string& map, bool world, bool picture = false, bool starting = false);
    bool loading() const { return loading_; }
    const std::string& map() const { return map_; }  // the map being loaded
    double loadMs() const { return loadMs_; }         // at Ended: from its loading picture to its world
    double beforeMs() const { return beforeMs_; }     // the old map's last frame to the loading picture; < 0: none
    // 3.0.2-beta.15: at Dropped, why a map start was not measured; "" for a moment without the world
    const char* droppedWhy() const { return why_; }

private:
    bool loading_ = false;
    bool pictured_ = false;   // the game's loading picture shown in it
    bool started_ = false;    // the game's map start ran in it
    const char* why_ = "";
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
