#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/ai/kinds.hpp"

namespace uagfx::ai {

// upscaled.grf for the settings app's "Ready textures" page: its pictures by map and kind (the 0.12 tags,
// texture_cache.hpp), and which entries stay when some are deleted.
struct CacheEntry {
    std::string name;
    uint32_t bytes = 0;  // stored (compressed)
};

struct CacheRow {
    std::string map;              // "" for the pictures seen on no map yet (made before 0.12)
    uint32_t count[kKinds] = {};  // pictures of the settings in force, by kind
    uint32_t total = 0;           // with the ones of no kind (made before 0.12)
    uint64_t bytes = 0;
};

struct CacheIndex {
    std::vector<CacheRow> rows;  // the maps by name, then "" when there are such pictures
    CacheRow all;                // every picture of the settings in force once (a texture on two maps is on two rows)
    uint32_t outdated = 0;       // pictures no setting in force uses: other settings, older versions
    uint64_t outdatedBytes = 0;
};

struct CacheDeletion {
    std::set<std::string> maps;  // the rows whose pictures go ("" the row of no map), every settings' pictures
    bool kinds[kKinds] = {true, true, true, true};  // of these kinds; a picture of no kind goes with its row
    bool outdated = false;       // and every outdated picture
};

class CacheContents {
public:
    explicit CacheContents(std::vector<CacheEntry> entries);
    // current: the folder each kind's pictures have with the settings in force (netFolder).
    CacheIndex index(const std::string current[kKinds]) const;
    // The names that stay: the tags always, the pictures `d` does not take.
    std::vector<std::string> keep(const CacheDeletion& d, const std::string current[kKinds]) const;
    size_t pictures() const { return pictures_; }

private:
    bool outdated(const std::string& folder, const std::string& key, const std::string current[kKinds]) const;
    Kind kindOf(const std::string& key) const;

    std::vector<CacheEntry> entries_;
    std::map<std::string, Kind> kinds_;                  // key -> kind
    std::map<std::string, std::set<std::string>> maps_;  // key -> maps
    size_t pictures_ = 0;
};

}  // namespace uagfx::ai
