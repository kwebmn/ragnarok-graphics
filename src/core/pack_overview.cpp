#include "core/pack_overview.hpp"

#include <algorithm>

namespace uagfx::pack {

Overview overviewOf(const std::vector<ZipEntry>& entries, uint64_t fileSize, const Index& index, uint32_t orphans,
                    uint32_t missing) {
    Overview o;
    o.bytes = fileSize;
    o.orphans = orphans;
    o.missing = missing;
    uint64_t used = 0, end = 0;
    for (const ZipEntry& e : entries) {
        const uint64_t size = 30 + e.name.size() + e.compressed;  // its local header (no extra field) and its data
        used += size;
        end = std::max(end, e.header + size);
        if (e.name == kIndexName) continue;
        ++o.pictures;
        Group g = Group::World;
        std::string key;
        const auto it = index.pictures.find(e.name);
        o.byGroup[keyOfPicture(e.name, g, key) ? groupName(g) : "?"]++;
        o.byModel[it == index.pictures.end() || it->second.model.empty() ? "?" : it->second.model]++;
        o.byKind[it == index.pictures.end() ? "?" : ai::kindName(it->second.kind)]++;
    }
    o.unread = end > used ? end - used : 0;
    return o;
}

}  // namespace uagfx::pack
