#pragma once
#include <cstddef>
#include <cstdint>
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
    bool open(const std::string& dir, std::string& error);
    const std::vector<std::string>& names() const { return names_; }  // lower case, backslashes, each once, sorted
    bool read(const std::string& name, std::vector<uint8_t>& out);
    std::string report() const;  // the archives and their files: for the log

private:
    struct Place {
        int archive = 0;
        std::string stored;  // its name in a GRF (its own case)
    };
    std::vector<std::unique_ptr<grf::Archive>> grfs_;
    std::vector<std::unique_ptr<zip::Index>> zips_;
    std::vector<std::pair<bool, int>> order_;  // per archive: a zip?, its index in grfs_ or zips_
    std::vector<std::string> paths_;
    std::vector<size_t> counts_;
    std::unordered_map<std::string, Place> where_;
    std::vector<std::string> names_;
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
    int minSide = 16, maxSide = 512;  // the world's textures ([ai] min_size, max_size)
};

// A file's part in the pack.
struct Source {
    enum Type : uint8_t { None, Texture, Tga, Interface, Sprite } type = None;
    ai::Kind kind = ai::Kind::None;  // what the driver takes it as: the network for it, the index's kind
};
Source sourceOf(const std::string& name, const Choice& c);  // a normal name ("data\texture\...")

// The pictures of a file the driver would give the network, in order: none of soft light (the driver leaves those as
// the game's), the world's within the sides, the interface's of an icon's or a picture's size (pieces are xBR's).
std::vector<Picture> picturesOf(const Source& s, const uint8_t* data, size_t size, const Choice& c);

// How the keys the builder makes stand against the pictures upscaled.grf holds from the game: per kind (its tags),
// how many of its pictures one of the game's files gives.
struct KeyCheck {
    uint32_t seen[ai::kKinds] = {}, found[ai::kKinds] = {};
    uint32_t untagged = 0, untaggedFound = 0;
    std::vector<std::string> missing;  // some of the ones no file gives: "sprites 64x64\0123..."
};
KeyCheck checkKeys(const std::vector<std::string>& cacheNames, const std::unordered_set<std::string>& made);
std::string keyCheckText(const KeyCheck& k);  // one line for the log and the settings app

}  // namespace uagfx::pack
