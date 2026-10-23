#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/ai/kinds.hpp"
#include "core/pack_source.hpp"

namespace uagfx::grf {
class Archive;
}
namespace uagfx::zip {
class Index;
}

namespace uagfx::pack {

// 0.43, the pack builder's look at the game: the archives DATA.INI lists ([Data] in the order of their numbers; GRF,
// GPF and ZIP) - the first that holds a file gives it, as the game reads them.
std::vector<std::string> dataArchives(const std::string& iniText);

class GameFiles {
public:
    GameFiles();
    ~GameFiles();
    // dir: the game's folder (its DATA.INI). false when no archive opens.
    bool open(const std::string& dir, std::string& error) { return open(dir, {}, error); }
    // 3.0.2-beta.5: and the GRFs of its updates folder before DATA.INI's (their names; core/updates.hpp), as the
    // game looks in them
    bool open(const std::string& dir, const std::vector<std::string>& updates, std::string& error);
    // 2.3 (N3): the archives `other` has open, for another thread - their tables and the names shared, a file handle
    // of its own for each GRF. false when other has none open.
    bool openLike(const GameFiles& other, std::string& error);
    const std::vector<std::string>& names() const;  // lower case, backslashes, each once, sorted
    // 2.2.5: the maps among the archives' files (data\<map>.rsw), sorted; their .gnd, .rsw and .rsm read too
    const std::vector<std::string>& maps() const;
    // 2.3: DATA.INI's archives that opened, in its order; the archives holding a file - the one that gives it first,
    // then those it shadows (empty: none)
    const std::vector<std::string>& paths() const;
    std::vector<int> archivesOf(const std::string& name) const;
    bool read(const std::string& name, std::vector<uint8_t>& out);
    std::string report() const;  // the archives and their files: for the log

private:
    struct Place {
        int archive = 0;
        std::string stored;  // its name in a GRF (its own case)
    };
    // What every reader of the same archives shares (openLike): read once, never changed after open().
    struct Shared {
        std::vector<std::unique_ptr<grf::Archive>> grfs;  // the first reader's; others open them like these
        std::vector<std::shared_ptr<zip::Index>> zips;     // a ZIP's index holds no file: shared as it is
        std::vector<std::pair<bool, int>> order;           // per archive: a zip?, its index in grfs or zips
        std::vector<std::string> paths;
        std::vector<std::string> failed;  // 1.6.5: DATA.INI's archives that did not open, and why
        std::vector<size_t> counts;
        std::unordered_map<std::string, Place> where;
        std::unordered_map<std::string, std::vector<int>> shadowed;  // 2.3: a name -> the other archives with it
        std::vector<std::string> names, maps;
    };
    std::shared_ptr<Shared> shared_;
    std::vector<std::unique_ptr<grf::Archive>> grfs_;  // openLike's own handles (open(): empty, shared_'s used)
};

// What the builder makes (the settings app's choices) and the sides the driver's AI takes.
struct Choice {
    bool world = true;      // the maps' ground and models
    bool effects = true;    // effect textures and effect sprites
    bool monsters = true;   // sprites by their files (sprite_exe::sourceOf)
    bool players = true;
    bool npcs = true;
    bool items = true;
    bool interface = true;  // the windows' icons and pictures, the cursor
    // 0.56: the bodies' frames in every clothes colour too (data\palette\몸\) - 4 to 9 times their frames: off
    // unless asked for ("bodycolours")
    bool bodyColours = false;
    int minSide = 16, maxSide = 512;  // the world's textures ([ai] min_size, max_size)
    // 2.2.5: the colours the server lets be worn (uAthena battle/client.conf max_hair_color, max_cloth_color) - the
    // client has 128 hair colours a style and up to 700 clothes colours a job; -1: every one there is. 2.5.19: hair
    // 0-20 - the game's stylist (npc/custom/dye.txt) dyes those, whatever the repository's client.conf says (8)
    int maxHairColour = 20, maxClothColour = 77;
};
// The kinds as the settings app passes them to the builder: "world,effects,monsters,players,npcs,items,interface"
// (the sides come with the [ai] settings).
std::string choiceText(const Choice& c);
Choice parseChoice(const std::string& words);  // the words it does not know are left out
bool anyChosen(const Choice& c);

// What the builder does, in a file the settings app reads (uaro-graphics\pack-status.txt): a "key=value" line each.
struct Status {
    // 0.48: the keys' check against upscaled.grf (0.43-0.47) is gone with the file: the game's log says what the
    // pack lacks.
    enum State : uint8_t { Starting, Reading, Building, Done, Stopped, Failed } state = Starting;
    uint32_t filesDone = 0, filesTotal = 0;  // the game's files read
    uint32_t made = 0, toMake = 0;           // pictures, this run
    uint32_t inPack = 0;                     // pictures in the pack now
    uint64_t bytes = 0;                      // the pack's size
    double rate = 0;                         // pictures a second, of late
    int eta = -1;                            // seconds left; -1 not known yet
    uint32_t pid = 0;                        // the builder's process
    std::string current;                     // the file it makes pictures of (UTF-8)
    std::string message;                     // why it failed
    std::string work;                        // 2.2.5: "build" or "drop" (made counts those taken out); "" older
    // 2.3 (uaRO Studio's N1): the stage - archives, textures, colours, making, writing ("" older) - and each reading
    // thread's file now ("" between files)
    std::string stage;
    std::vector<std::string> threads;
};
std::string statusText(const Status& s);
bool parseStatus(const std::string& text, Status& out);  // false: no state in it


// A file's part in the pack.
struct Source {
    enum Type : uint8_t { None, Texture, Tga, Interface, Sprite, Jpeg } type = None;  // 0.49: Jpeg
    ai::Kind kind = ai::Kind::None;  // what the driver takes it as: the network for it, the index's kind
};
Source sourceOf(const std::string& name, const Choice& c);  // a normal name ("data\texture\...")

// The pictures of a file the driver would give the network, in order: none of soft light (the driver leaves those as
// the game's), the world's within the sides, the interface's of an icon's or a picture's size (pieces are xBR's).
// 0.49: a sprite's frames with another palette (1024 bytes, RGBA): a head's in its hair colour (palettesOf).
std::vector<Picture> picturesOf(const Source& s, const uint8_t* data, size_t size, const Choice& c,
                                const uint8_t* palette = nullptr);
// 0.49: the palettes the game draws a sprite with besides its own, among the game's files (sorted names): a head's
// hair colours - data\palette\머리\머리<N>_<sex>_<colour>.pal for data\sprite\인간족\머리통\<sex>\<N>_<sex>.spr.
// 2.2.5: maxHair, maxCloth - none past these colours (-1: every one).
std::vector<std::string> palettesOf(const std::string& sprite, const std::vector<std::string>& names,
                                    bool bodies = false, int maxHair = -1, int maxCloth = -1);

}  // namespace uagfx::pack
