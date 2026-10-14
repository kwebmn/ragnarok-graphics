#pragma once
#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "core/map_axes.hpp"
#include "core/rsw.hpp"

namespace uagfx::gpu {

// 0.25: the map the game shows, as its .rsw describes it - the lamps' lights its makers placed and its effects (the
// game shows effect ids up to 629 only) - read in the background from the client's data: the data folder, then the
// archives DATA.INI lists, in their order (zips and GRFs). And the axes that put the file's places into the game's
// world (core/map_axes.hpp), found from the torches seen and kept in uaro-graphics\map-axes.txt for the next runs.
class MapWorld {
public:
    MapWorld();
    ~MapWorld();
    MapWorld(const MapWorld&) = delete;
    MapWorld& operator=(const MapWorld&) = delete;

    void setGameDir(const std::string& dir);  // the game's folder: its data and the axes file
    void setMap(const std::string& name);     // "prontera"; "" none
    void poll();                              // the render thread, each frame: takes a finished read
    const rsw::World* world() const { return ready_ ? &world_ : nullptr; }
    map::Axes axes() const { return axes_; }
    // The base of a torch's flame seen this frame, in the game's world: the axes are found from them.
    void noteTorch(const map::Point& world);

private:
    struct Shared;
    static DWORD WINAPI threadMain(void* self);
    void loop();
    void tryAxes();

    std::unique_ptr<Shared> shared_;  // with the loader's thread
    HANDLE thread_ = nullptr;
    std::string gameDir_;
    std::string shown_;  // the map world_ is of
    rsw::World world_;
    bool ready_ = false;
    map::Axes axes_;
    std::vector<map::Point> placed_;  // the file's torches (effect 47)
    std::vector<map::Point> seen_;    // where torches were seen, until the axes are known
};

}  // namespace uagfx::gpu
