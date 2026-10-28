#include "core/reference_index.hpp"

#include <algorithm>
#include <cctype>

#include "core/json.hpp"
#include "miniz.h"

namespace uagfx::refindex {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool has(const std::string& s, std::initializer_list<const char*> words) {
    for (const char* w : words)
        if (s.find(w) != std::string::npos) return true;
    return false;
}

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

}  // namespace

bool gunzip(const std::string& gz, std::string& out) {
    const auto* p = reinterpret_cast<const unsigned char*>(gz.data());
    const size_t n = gz.size();
    if (n < 18 || p[0] != 0x1F || p[1] != 0x8B || p[2] != 8) return false;  // deflate only
    const unsigned flags = p[3];
    size_t at = 10;
    if (flags & 4) {  // FEXTRA
        if (at + 2 > n) return false;
        at += 2 + size_t(p[at] | (p[at + 1] << 8));
    }
    for (unsigned f : {8u, 16u}) {  // FNAME, FCOMMENT: zero-ended
        if (!(flags & f)) continue;
        while (at < n && p[at]) ++at;
        ++at;
    }
    if (flags & 2) at += 2;  // FHCRC
    if (at + 8 > n) return false;
    auto le32 = [&](size_t i) { return uint32_t(p[i]) | uint32_t(p[i + 1]) << 8 | uint32_t(p[i + 2]) << 16 |
                                       uint32_t(p[i + 3]) << 24; };
    const uint32_t size = le32(n - 4), crc = le32(n - 8);
    if (size > (64u << 20)) return false;  // not a pack of ours
    std::string data(size, '\0');
    const size_t got = tinfl_decompress_mem_to_mem(data.data(), data.size(), p + at, n - 8 - at, 0);
    if (got == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED || got != size) return false;
    if (mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const unsigned char*>(data.data()), data.size()) != crc) return false;
    out.swap(data);
    return true;
}

NpcKind kindOf(const std::string& name) {
    const std::string n = lower(name);
    if (has(n, {"kafra"})) return NpcKind::Kafra;
    if (has(n, {"heal", "priest", "nurse", "forgetful"})) return NpcKind::Healer;
    if (has(n, {"tool", "item", "dealer", "merchant", "trader", "shop"})) return NpcKind::Tool;
    if (has(n, {"weapon", "blacksmith", "smith", "forge"})) return NpcKind::Weapon;
    if (has(n, {"armor", "armour"})) return NpcKind::Armor;
    if (has(n, {"inn", "innkeeper", "bed", "hotel"})) return NpcKind::Inn;
    if (has(n, {"guide", "info", "wanderer", "signpost", "guard", "soldier"})) return NpcKind::Guide;
    if (has(n, {"job", "master", "guru", "teacher", "trainer"})) return NpcKind::Job;
    if (has(n, {"warp", "airship", "airport", "ship", "boat", "sailor", "captain"})) return NpcKind::Warp;
    return NpcKind::Other;
}

uint32_t colorOf(NpcKind k) {
    switch (k) {
        case NpcKind::Kafra: return 0xD6336C;
        case NpcKind::Healer: return 0x37B24D;
        case NpcKind::Tool: return 0x1971C2;
        case NpcKind::Weapon: return 0xE8590C;
        case NpcKind::Armor: return 0x7048E8;
        case NpcKind::Inn: return 0x0CA678;
        case NpcKind::Guide: return 0xF08C00;
        case NpcKind::Job: return 0x1098AD;
        case NpcKind::Warp: return 0x4263EB;
        case NpcKind::Other: break;
    }
    return 0x868E96;
}

bool build(const std::string& text, Index& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object || v["v"].asInt(0) != 1) return false;
    Index ix;
    ix.data = v["data"].asInt(0);
    ix.commit = v["commit"].asString();
    for (const auto& m : v["mobs"].members()) {  // {id: [name, lv, hp, exp, jexp, size, race, ele, ele lv, boss, …]}
        const int id = std::atoi(m.first.c_str());
        if (id <= 0 || m.second.items().size() < 10) continue;
        Mob mob;
        mob.name = m.second[size_t(0)].asString();
        mob.lv = int(m.second[size_t(1)].asInt(0));
        mob.boss = m.second[size_t(9)].asInt(0) != 0;
        ix.mobs[id] = std::move(mob);
    }
    for (const auto& m : v["spawns"].members()) {  // {map: [[mob, count, x, y, xs, ys, respawn ms]]}
        MapHints& h = ix.maps[m.first];
        for (const json::Value& s : m.second.items()) {
            const int id = int(s[size_t(0)].asInt(0));
            if (!ix.mobs.count(id)) continue;  // not one the site lists
            auto it = std::find_if(h.mobs.begin(), h.mobs.end(), [&](const MobOnMap& o) { return o.id == id; });
            if (it == h.mobs.end()) it = h.mobs.insert(h.mobs.end(), MobOnMap{id, 0, 0});
            it->count += int(s[size_t(1)].asInt(0));
            it->respawnMs = std::max(it->respawnMs, int(s[size_t(6)].asInt(0)));
        }
    }
    for (const json::Value& r : v["routes"].items()) {  // [map, x, y, dest, dx, dy, kind w|k|s, name, price, flags]
        const std::string map = r[size_t(0)].asString(), dest = r[size_t(3)].asString();
        if (r[size_t(6)].asString() != "w" || map.empty() || dest.empty() || dest == map) continue;
        ix.maps[map].warps.push_back(Warp{int(r[size_t(1)].asInt(0)), int(r[size_t(2)].asInt(0)), dest,
                                          int(r[size_t(4)].asInt(0)), int(r[size_t(5)].asInt(0))});
    }
    for (const json::Value& n : v["npcs"].items()) {  // [map, x, y, name, sprite] - the visible ones
        const std::string map = n[size_t(0)].asString();
        if (map.empty()) continue;
        Npc npc{int(n[size_t(1)].asInt(0)), int(n[size_t(2)].asInt(0)), n[size_t(3)].asString(),
                int(n[size_t(4)].asInt(0))};
        npc.kind = kindOf(npc.name);
        ix.maps[map].npcs.push_back(std::move(npc));
    }
    for (auto& m : ix.maps) {
        std::stable_sort(m.second.mobs.begin(), m.second.mobs.end(), [&](const MobOnMap& a, const MobOnMap& b) {
            const Mob& ma = ix.mobs[a.id];
            const Mob& mb = ix.mobs[b.id];
            if (ma.boss != mb.boss) return ma.boss;
            if (a.count != b.count) return a.count > b.count;
            return ma.name < mb.name;
        });
    }
    out = std::move(ix);
    return true;
}

std::map<std::string, std::string> mapNames(const std::string& text) {
    std::map<std::string, std::string> out;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        const size_t hash = line.find('#');
        if (hash == std::string::npos) continue;
        std::string code = lower(trim(line.substr(0, hash)));
        if (code.size() > 4 && code.compare(code.size() - 4, 4, ".rsw") == 0) code.resize(code.size() - 4);
        const size_t close = line.find('#', hash + 1);
        const size_t len = close == std::string::npos ? std::string::npos : close - hash - 1;
        const std::string name = trim(line.substr(hash + 1, len));
        if (!code.empty() && !name.empty()) out[code] = name;
    }
    return out;
}

std::string shortName(const std::string& name) {
    return trim(name.substr(0, name.find(',')));
}

}  // namespace uagfx::refindex
