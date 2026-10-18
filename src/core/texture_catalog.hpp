#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/atlas_tiles.hpp"
#include "core/pack_build.hpp"
#include "core/texture_pack.hpp"

namespace uagfx::catalog {

// 2.3, uaRO Studio (T1): every texture of the game as the texture pack sees it - a file of the game's archives, the
// archive that gives it and those it shadows, the pictures the pack would hold of it (a picture is named by its
// pixels, never its file: this is the only place a picture meets its file), the maps it is on and how a ground
// texture lies there. Built by threads (start, then decode each file anywhere, then mapsOf), kept in the studio's
// catalog.bin, and brought up to date by the archives' sizes and times: an archive that changed is read again.

using Read = std::function<bool(const std::string& name, std::vector<uint8_t>& out)>;

struct ArchiveStamp {
    std::string path;  // as DATA.INI names it
    uint64_t size = 0;
    int64_t time = 0;
    bool operator==(const ArchiveStamp& o) const { return path == o.path && size == o.size && time == o.time; }
};

struct PictureOf {
    std::string key;                   // ai::keyName
    uint8_t group = 0, aiGroup = 0;    // pack::Group, ai::Group
    uint16_t width = 0, height = 0;
    std::string palette;               // "" its own; else the palette file (a colour the server lets be worn)
    std::vector<std::string> lowKeys;  // the world's at the half and the quarter texture detail
};

struct FileEntry {
    std::string name;                   // CP949, lower case, backslashes
    std::vector<uint16_t> archives;     // the giving one first, then the shadowed
    uint8_t type = 0, kind = 0;         // pack::Source
    std::vector<PictureOf> pictures;
    std::vector<std::string> palettes;  // a sprite's colours it was decoded with
    std::vector<uint32_t> maps;         // into Catalog::maps
    atlas_tiles::Grid grid;
};

struct Catalog {
    std::vector<ArchiveStamp> archives;
    std::vector<std::string> maps;
    std::vector<FileEntry> files;
    int maxHair = 8, maxCloth = 77;
};

// The files of the listing (each name with its archives, as GameFiles::archivesOf gives them) that have a part in the
// pack, sorted. An old catalog's entry is kept as it was when its archives are the same as they were (and a sprite's
// colours too); toDecode: the files left to decode.
Catalog start(const std::vector<ArchiveStamp>& archives,
              const std::vector<std::pair<std::string, std::vector<uint16_t>>>& names, const pack::Choice& choice,
              const Catalog* old, std::vector<size_t>& toDecode);
// A file's pictures (allNames: every file of the listing, sorted - a sprite's colours are found among them). false:
// stopped; a file that cannot be read or decoded has none.
bool decode(FileEntry& f, const std::vector<std::string>& allNames, const Read& read, const pack::Choice& choice,
            const std::atomic<bool>* stop);
// The maps (GameFiles::maps) each file is on - by their ground and their models - and each ground texture's grid.
void mapsOf(Catalog& c, const std::vector<std::string>& maps, const Read& read, const std::atomic<bool>* stop);

// A key (or a lower detail's key) to its files' pictures: (file, picture).
std::unordered_multimap<std::string, std::pair<size_t, size_t>> byKey(const Catalog& c);

// The pack as the catalog meets it: its pictures with their times, its index.
struct PackView {
    std::unordered_map<std::string, int64_t> pictures;
    pack::Index index;
};
enum class State : uint8_t { Missing, InPack, OwnAlways };
State stateOf(const PictureOf& p, const PackView& v);  // a key whose alias is a picture of the pack: in it
std::vector<std::string> orphans(const Catalog& c, const PackView& v);  // pictures no key of the catalog names

std::string save(const Catalog& c);                 // "UCAT", its version, then the fields
bool load(const std::string& bytes, Catalog& out);  // false: another version, cut short or broken

}  // namespace uagfx::catalog
