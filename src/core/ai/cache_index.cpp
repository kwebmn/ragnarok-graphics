#include "core/ai/cache_index.hpp"

#include <utility>

namespace uagfx::ai {

CacheIndex CacheContents::index() const {
    CacheIndex out;
    std::map<std::string, CacheRow> rows;
    const auto add = [](CacheRow& r, Kind k, uint64_t bytes) {
        if (int(k) < kKinds) ++r.count[int(k)];
        ++r.total;
        r.bytes += bytes;
    };
    for (const CacheEntry& e : entries_) {
        add(out.all, e.kind, e.bytes);
        if (e.maps.empty()) {
            add(rows[""], e.kind, e.bytes);
            continue;
        }
        for (const std::string& map : e.maps) add(rows[map], e.kind, e.bytes);
    }
    for (auto& r : rows) {
        r.second.map = r.first;
        if (!r.first.empty()) out.rows.push_back(r.second);
    }
    const auto none = rows.find("");
    if (none != rows.end()) out.rows.push_back(none->second);
    return out;
}

std::vector<std::string> CacheContents::keep(const CacheDeletion& d) const {
    std::vector<std::string> out;
    for (const CacheEntry& e : entries_) {
        bool gone = false;
        if (int(e.kind) >= kKinds || d.kinds[int(e.kind)]) {
            if (e.maps.empty()) {
                gone = d.maps.count("") > 0;
            } else {
                for (const std::string& map : e.maps) gone = gone || d.maps.count(map) > 0;
            }
        }
        if (!gone) out.push_back(e.name);
    }
    return out;
}

}  // namespace uagfx::ai
