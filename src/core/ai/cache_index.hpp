#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/ai/kinds.hpp"

namespace uagfx::ai {

// The texture pack (uaro-textures.zip) for the settings app's "Ready textures" page: its pictures by map and kind
// (0.48: from the pack's index - its builder's pictures are of no map, the game's own making notes the maps), and
// which pictures stay when some are deleted. Until 0.47 the page showed upscaled.grf this way.
struct CacheEntry {
    std::string name;
    uint64_t bytes = 0;  // in the pack
    Kind kind = Kind::None;  // the index's; None: not in it
    std::set<std::string> maps;
};

struct CacheRow {
    std::string map;              // "" for the pictures of no map
    uint32_t count[kKinds] = {};  // pictures by kind
    uint32_t total = 0;           // with the ones of no kind
    uint64_t bytes = 0;
};

struct CacheIndex {
    std::vector<CacheRow> rows;  // the maps by name, then "" when there are such pictures
    CacheRow all;                // every picture once (a texture on two maps is on two rows)
};

struct CacheDeletion {
    std::set<std::string> maps;  // the rows whose pictures go ("" the row of no map)
    bool kinds[kKinds] = {true, true, true, true, true};  // of these kinds; a picture of no kind goes with its row
};

class CacheContents {
public:
    explicit CacheContents(std::vector<CacheEntry> entries) : entries_(std::move(entries)) {}
    CacheIndex index() const;
    std::vector<std::string> keep(const CacheDeletion& d) const;  // the pictures that stay
    size_t pictures() const { return entries_.size(); }

private:
    std::vector<CacheEntry> entries_;
};

}  // namespace uagfx::ai
