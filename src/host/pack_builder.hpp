#pragma once
#include <array>
#include <string>

#include "core/ai/groups.hpp"
#include "core/config.hpp"
#include "core/pack_build.hpp"

namespace uagfx::host {

// 0.43: uaro-render64.exe --pack build - the texture pack (uaro-textures.zip) made on this computer's graphics
// card for the settings app's "Texture pack" page, which starts it, reads its status file twice a second and asks it
// to stop by a file of its own (the status file's name and ".stop"). 2.2.5: --pack drop - pictures taken out of it.
struct PackArgs {
    std::string mode;    // "build": the pack (0.48: "check", the keys against upscaled.grf, is gone with the file)
    std::string dir;     // the game's folder
    std::string out;     // the pack; uaro-textures.zip in the game's folder when empty
    std::string status;  // uaro-graphics\pack-status.txt in the game's folder when empty
    pack::Choice choice;
    AiSettings ai;       // the networks (model, sprite model, denoise) and the world's sides
    int quality = 90;    // WebP: 1-99 lossy, 100 lossless
    bool fresh = false;  // a new pack in place of the one there; else the one there goes on
    // 1.6, --redo world,players...: of these groups the pictures another model made than the group's are made again
    // (ai::modelOfEntry), each into its place in the pack.
    std::array<bool, ai::kGroups> redo{};
    // 2.2.5, --map cmd_fild02 [--map-kinds ground,models]: one map's textures alone (core/map_textures.hpp) - with
    // --remake 1 every picture of them made again (by the models of --ai), or (--pack drop) taken out of the pack:
    // a model tried on one place from the Texture pack page.
    std::string map;
    bool mapGround = true, mapModels = true;
    bool remake = false;
    // 2.3 (uaRO Studio's N2), --catalog <file>: the studio's texture catalog (core/texture_catalog.hpp) - when it is
    // up to date with the game's archives, its keys stand for the reading pass (no file of the game read twice).
    std::string catalog;
    std::string catalogData;  // or its bytes (the studio's engine reads the file by its wide name)
};

// 2.3: where a build tells how it goes and hears a stop - the status file and its ".stop" file (the settings app), or
// uaRO Studio's pipe (its engine runs the build).
struct BuildSink {
    virtual ~BuildSink() = default;
    virtual void write(const pack::Status& s, bool now) = 0;  // now: at once, else at most every half second
    virtual bool stopAsked() = 0;
};

int runPack(const PackArgs& a, BuildSink* sink = nullptr);  // the exit code: 0 done or stopped; null: the file

}  // namespace uagfx::host
