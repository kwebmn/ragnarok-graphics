#pragma once
#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "core/cave_vault.hpp"
#include "core/grass_beds.hpp"
#include "core/ground_heights.hpp"
#include "core/map_axes.hpp"
#include "core/rsw.hpp"
#include "win/gpu/own_ground_data.hpp"

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
    // 3.0.18-beta.252 (R-9UNG8S «в замке трава плоская с отражением»): whether the ground at the world's x and z is a
    // hall's floor (ground_heights::interiorTexture: a castle's carpet, its green tiles) - false not known; known
    // once the .gnd is read and the axes found
    bool insideAt(float x, float z) const;
    bool hallsKnown() const { return ground() && axes_.known() && !ground_.inside.empty(); }
    // 3.0.20-beta.46 (R-HGTAKK «в пронте мигает трава, но по идее ее на асфальте быть не должно»): whether the ground
    // at the world's x and z is of a texture that is not green (ground_cells::grassyCells: paving, sand, rock) - no
    // grass's ground; false not known (the map's pictures are read with the own ground alone). The axes not found
    // yet: the file's own, as the own ground is drawn till then
    bool bareAt(float x, float z) const;
    bool bareKnown() const { return ground() && !ground_.bare.empty(); }
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
    // 3.0.20-beta.46 (R-3T9MZC «колоны как будто в воздухе»): a dungeon's posts (cave_vault::postOf of its models:
    // tall, wide-topped), the map file's places - the 3D view's vault comes down to them; none not read, no dungeon
    const std::vector<cave_vault::Post>& posts() const { return posts_; }
    // 3.0.20-beta.52: indoors (a house, a castle's hall) the walls' height by the map's tall models (cave_vault::
    // wallsByModels) - the 3D view's ceiling lies over it where the ground makes no walls; 0 not known, not indoors
    float wallsHeight() const { return walls_; }
    // 3.0.20-beta.54 (the owner's «делай 1-9», 5, part 2): the ceiling's colour by the room's walls - the upper half
    // of the map's tall models, in their textures' mean colours (cave_vault::wallTone); false: not known
    bool wallsColour(float out[3]) const {
        for (int k = 0; k < 3; ++k) out[k] = wallsColour_[k];
        return wallsColourKnown_;
    }
    // 3.0.20-beta.54: a dungeon's own rock for the 3D view's vault (cave_vault::rockPatch): a patch of one of its
    // ground's sheets with its levels (none: empty), the patch's texels a unit of the world, its mean colour (`known`
    // - the rock's own where no patch was found: the vault's noise in it)
    struct VaultRock {
        OwnGround::Picture patch;
        float texels = 6.4f;
        float mean[3] = {0.0f, 0.0f, 0.0f};
        bool known = false;
    };
    const VaultRock& vaultRock() const { return rock_; }
    // 3.0.20-beta.57 (R-J3GKD9 «тут другие комнаты видны»): a house's rooms (core/rooms) - for the 3D view's ceiling
    // over each room's own floor and for drawing the one the player is in alone; null: none known, not indoors
    const rooms::Map* rooms() const { return rooms_.count > 0 ? &rooms_ : nullptr; }
    // 3.0.4-beta.10: the own ground (own_ground_data.hpp) read with the map while it is wanted - turned on for a map
    // read without it, the map is read again; none not wanted, not read yet, or no .gnd. Its pictures let go once the
    // renderer has them (ownPicturesUp).
    void setOwnGround(bool wanted);
    // 3.0.20-beta.71: [view3d] room_walls - a house's black blocks beside its rooms dressed in the rooms' wall
    // pictures as the own ground is read (core/room_blocks); a change shows from the next map read
    void setRoomWalls(bool wanted);
    const OwnGround* ownGround() const { return own_ && own_->map == shown_ ? own_.get() : nullptr; }
    void ownPicturesUp();
    // 3.0.6-beta.3: the last read asked for not taken yet (asked, under way, or done and not polled) - «Тест графіки»
    // waits for the own world's
    bool reading() const { return asked_ != taken_; }

private:
    struct Shared;
    static DWORD WINAPI threadMain(void* self);
    void loop();
    void tryAxes();
    void ask(const std::string& name);  // the loader's thread reads it

    std::unique_ptr<Shared> shared_;  // with the loader's thread
    HANDLE thread_ = nullptr;
    std::string gameDir_;
    std::string shown_;  // the map world_ is of
    rsw::World world_;
    ground_heights::Ground ground_;  // 2.6.18
    std::vector<grass_beds::Box> beds_;  // 3.0.1-beta.3: in the file's places
    std::vector<cave_vault::Post> posts_;  // 3.0.20-beta.46
    float walls_ = 0.0f;                   // 3.0.20-beta.52
    float wallsColour_[3] = {0.0f, 0.0f, 0.0f};  // 3.0.20-beta.54
    bool wallsColourKnown_ = false;
    VaultRock rock_;
    rooms::Map rooms_;  // 3.0.20-beta.57
    bool ready_ = false;
    uint32_t reads_ = 0;  // 2.7.57: the maps read so far (laidKey)
    map::Axes axes_;
    std::vector<map::Point> placed_;  // the file's torches (effect 47)
    std::vector<rsw::Light> lamps_;
    std::vector<map::Point> seen_;    // where torches were seen, until the axes are known
    bool ownWanted_ = false;          // 3.0.4-beta.10
    bool roomWalls_ = true;           // 3.0.20-beta.71
    uint32_t asked_ = 0, taken_ = 0;  // 3.0.6-beta.3: the reads' numbers - the last asked for, the last taken
    std::unique_ptr<OwnGround> own_;
    uint32_t ownSerial_ = 0;
};

}  // namespace uagfx::gpu
