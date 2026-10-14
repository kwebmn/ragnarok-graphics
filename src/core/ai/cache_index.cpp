#include "core/ai/cache_index.hpp"

#include <utility>

#include "core/ai/texture_cache.hpp"

namespace uagfx::ai {

CacheContents::CacheContents(std::vector<CacheEntry> entries) : entries_(std::move(entries)) {
    for (const CacheEntry& e : entries_) {
        const ParsedEntry p = parseEntry(e.name);
        if (p.type == ParsedEntry::KindTag)
            kinds_.emplace(p.key, p.kind);  // the first one the driver wrote
        else if (p.type == ParsedEntry::MapTag)
            maps_[p.key].insert(p.map);
        else if (p.type == ParsedEntry::Picture)
            ++pictures_;
    }
}

Kind CacheContents::kindOf(const std::string& key) const {
    const auto it = kinds_.find(key);
    return it == kinds_.end() ? Kind::None : it->second;
}

bool CacheContents::outdated(const std::string& folder, const std::string& key,
                             const std::string current[kKinds]) const {
    const Kind k = kindOf(key);
    if (k != Kind::None) return folder != current[int(k)];
    for (int i = 0; i < kKinds; ++i)
        if (folder == current[i]) return false;
    return true;
}

CacheIndex CacheContents::index(const std::string current[kKinds]) const {
    CacheIndex out;
    std::map<std::string, CacheRow> rows;
    const auto add = [](CacheRow& r, Kind k, uint32_t bytes) {
        if (k != Kind::None) ++r.count[int(k)];
        ++r.total;
        r.bytes += bytes;
    };
    for (const CacheEntry& e : entries_) {
        const ParsedEntry p = parseEntry(e.name);
        if (p.type != ParsedEntry::Picture) continue;
        if (outdated(p.folder, p.key, current)) {
            ++out.outdated;
            out.outdatedBytes += e.bytes;
            continue;
        }
        const Kind k = kindOf(p.key);
        add(out.all, k, e.bytes);
        const auto m = maps_.find(p.key);
        if (m == maps_.end()) {
            add(rows[""], k, e.bytes);
            continue;
        }
        for (const std::string& map : m->second) add(rows[map], k, e.bytes);
    }
    for (auto& r : rows) {
        r.second.map = r.first;
        if (!r.first.empty()) out.rows.push_back(r.second);
    }
    const auto none = rows.find("");
    if (none != rows.end()) out.rows.push_back(none->second);
    return out;
}

std::vector<std::string> CacheContents::keep(const CacheDeletion& d, const std::string current[kKinds]) const {
    std::vector<std::string> out;
    for (const CacheEntry& e : entries_) {
        const ParsedEntry p = parseEntry(e.name);
        bool gone = false;
        if (p.type == ParsedEntry::Picture) {
            gone = d.outdated && outdated(p.folder, p.key, current);
            const Kind k = kindOf(p.key);
            if (!gone && (k == Kind::None || d.kinds[int(k)])) {
                const auto m = maps_.find(p.key);
                if (m == maps_.end()) {
                    gone = d.maps.count("") > 0;
                } else {
                    for (const std::string& map : m->second) gone = gone || d.maps.count(map) > 0;
                }
            }
        }
        if (!gone) out.push_back(e.name);
    }
    return out;
}

}  // namespace uagfx::ai
