#pragma once
#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "core/grass_beds.hpp"
#include "core/ground_heights.hpp"
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
    // 0.25.2: its lights but those on a torch - the makers put one on many torches to bake its glow into the ground,
    // and the torch lights that place itself.
    const std::vector<rsw::Light>& lamps() const { return lamps_; }
    map::Axes axes() const { return axes_; }
    // 2.6.18: its ground's heights (its .gnd, read with the .rsw) - where its walls stand; none not read
    const ground_heights::Ground* ground() const { return ready_ && !ground_.corners.empty() ? &ground_ : nullptr; }
    // 2.7.34: the ground's height (the world's y) at the world's x and z - false with no .gnd read, the axes not found
    // or off the map (the birds settle only over the water)
    bool groundAt(float x, float z, float& y) const;
    // 2.7.57: whether the ground at the world's x and z lies in a patch its makers laid by hand (ground_heights::
    // laidPatches) - false not known; laidKey() changes as what it says does (another read, the axes found), 0 never
    bool laidAt(float x, float z) const;
    uint64_t laidKey() const {
        return ground() && axes_.known() ? uint64_t(reads_) << 8 | uint64_t(axes_.x + 1) << 4 | uint64_t(axes_.z + 1)
                                         : 0;
    }
    // The base of a torch's flame seen this frame, in the game's world: the axes are found from them.
    void noteTorch(const map::Point& world);
    // 3.0.1-beta.3: the tops of its flower beds (core/grass_beds.hpp) in the game's world - the grass's ground too;
    // none not read or before the axes are known
    std::vector<grass_beds::Box> beds() const {
        return ready_ ? grass_beds::inWorld(beds_, axes_) : std::vector<grass_beds::Box>{};
    }

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
    ground_heights::Ground ground_;  // 2.6.18
    std::vector<grass_beds::Box> beds_;  // 3.0.1-beta.3: in the file's places
    bool ready_ = false;
    uint32_t reads_ = 0;  // 2.7.57: the maps read so far (laidKey)
    map::Axes axes_;
    std::vector<map::Point> placed_;  // the file's torches (effect 47)
    std::vector<rsw::Light> lamps_;
    std::vector<map::Point> seen_;    // where torches were seen, until the axes are known
};

}  // namespace uagfx::gpu
