#include "core/reference_index.hpp"

#include <algorithm>
#include <cctype>
#include <tuple>

#include "core/json.hpp"
#include "core/settings/search.hpp"
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

namespace {

int at(const json::Value& a, size_t i, int fallback = 0) { return int(a[i].asInt(fallback)); }

void readMobs(const json::Value& v, Index& ix) {
    // {id: [name, lv, hp, exp, jexp, size, race, ele, ele lv, boss, drops, mvp drops, card | null]}
    for (const auto& m : v["mobs"].members()) {
        const int id = std::atoi(m.first.c_str());
        const json::Value& a = m.second;
        if (id <= 0 || a.items().size() < 10) continue;
        Mob mob;
        mob.name = a[size_t(0)].asString();
        mob.lv = at(a, 1);
        mob.hp = at(a, 2);
        mob.baseExp = at(a, 3);
        mob.jobExp = at(a, 4);
        mob.size = at(a, 5);
        mob.race = at(a, 6);
        mob.element = at(a, 7);
        mob.elementLv = std::max(1, at(a, 8, 1));
        mob.boss = at(a, 9) != 0;
        for (uint8_t kind = 0; kind < 2; ++kind)
            for (const json::Value& d : a[size_t(10 + kind)].items())
                if (at(d, 0) > 0) mob.drops.push_back(Drop{at(d, 0), at(d, 1), kind});
        if (a[size_t(12)].type() == json::Value::Array && at(a[size_t(12)], 0) > 0)
            mob.drops.push_back(Drop{at(a[size_t(12)], 0), at(a[size_t(12)], 1), 2});
        ix.mobs[id] = std::move(mob);
    }
}

void readItems(const json::Value& v, Index& ix) {  // {id: [name, aegis, type, buy, weight, slots, loc]}
    for (const auto& m : v["items"].members()) {
        const int id = std::atoi(m.first.c_str());
        const json::Value& a = m.second;
        if (id <= 0 || a.items().size() < 6) continue;
        ix.items[id] = Item{a[size_t(0)].asString(), a[size_t(1)].asString(), at(a, 2), at(a, 3), at(a, 4), at(a, 5)};
    }
}

// What refers to what: each list in the order a card shows it
void link(Index& ix) {
    for (const auto& m : ix.mobs)
        for (const Drop& d : m.second.drops) ix.droppedBy[d.item].push_back(Source{m.first, d.rate, d.kind});
    for (auto& d : ix.droppedBy)
        std::sort(d.second.begin(), d.second.end(), [&](const Source& a, const Source& b) {
            if (a.rate != b.rate) return a.rate > b.rate;
            return ix.mobs[a.mob].name < ix.mobs[b.mob].name;
        });
    for (size_t s = 0; s < ix.shops.size(); ++s)
        for (const auto& it : ix.shops[s].items) ix.soldAt[it.first].emplace_back(int(s), it.second);
    for (auto& s : ix.soldAt)
        std::stable_sort(s.second.begin(), s.second.end(),
                         [](const auto& a, const auto& b) { return a.second < b.second; });
    for (const auto& m : ix.maps)
        for (const MobOnMap& o : m.second.mobs) ix.spawnsOf[o.id].push_back(Where{m.first, o.count, o.respawnMs});
    for (auto& w : ix.spawnsOf)
        std::sort(w.second.begin(), w.second.end(), [](const Where& a, const Where& b) {
            return a.count != b.count ? a.count > b.count : a.map < b.map;
        });
    for (size_t c = 0; c < ix.craft.size(); ++c) {
        ix.craftOf[ix.craft[c].item].push_back(int(c));
        for (const auto& mat : ix.craft[c].mats) ix.usedIn[mat.first].push_back(int(c));
    }
}

}  // namespace

bool build(const std::string& text, Index& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object || v["v"].asInt(0) != 1) return false;
    Index ix;
    ix.data = v["data"].asInt(0);
    ix.commit = v["commit"].asString();
    ix.date = v["date"].asString();
    readMobs(v, ix);
    readItems(v, ix);
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
        const std::string map = r[size_t(0)].asString(), dest = r[size_t(3)].asString(), kind = r[size_t(6)].asString();
        if (map.empty() || dest.empty()) continue;
        if (kind == "w" || kind == "k" || kind == "s") {  // 3.0.11-beta.8: the routes' moves, the ones within too
            const json::Value& price = r[size_t(8)];
            Move m{at(r, 1), at(r, 2), dest, at(r, 4), at(r, 5), kind == "w" ? How::Warp : kind == "k" ? How::Kafra
                                                                                                        : How::Npc,
                   r[size_t(7)].asString(), price.type() == json::Value::Number ? int(price.asInt(0)) : -1};
            m.paid = m.how == How::Kafra || (m.how == How::Npc && (m.price > 0 || (at(r, 9) & 2) != 0));
            m.conditional = (at(r, 9) & 1) != 0;
            ix.maps[map].moves.push_back(std::move(m));
        }
        if (dest == map) continue;
        if (kind == "w") {
            ix.maps[map].warps.push_back(Warp{at(r, 1), at(r, 2), dest, at(r, 4), at(r, 5)});
        } else if (kind == "k" || kind == "s") {  // 3.0.11-beta.7: Kafra's and the NPCs' teleports
            const json::Value& price = r[size_t(8)];
            const int zeny = price.type() == json::Value::Number ? int(price.asInt(0)) : -1;
            ix.maps[map].teleports.push_back(Teleport{at(r, 1), at(r, 2), r[size_t(7)].asString(), dest, at(r, 4),
                                                      at(r, 5), zeny, kind == "k", (at(r, 9) & 1) != 0});
        }
    }
    for (const json::Value& s : v["shops"].items()) {  // [map, x, y, name, [[item, price]]]
        Shop shop{s[size_t(0)].asString(), s[size_t(3)].asString(), at(s, 1), at(s, 2), {}};
        for (const json::Value& it : s[size_t(4)].items()) shop.items.emplace_back(at(it, 0), at(it, 1));
        if (!shop.map.empty() && !shop.items.empty()) ix.shops.push_back(std::move(shop));
    }
    for (const json::Value& p : v["pets"].items())  // [mob, lure, egg, equip, food]
        ix.pets.push_back(Pet{at(p, 0), at(p, 1), at(p, 2), at(p, 3), at(p, 4)});
    for (const json::Value& c : v["craft"].items()) {  // [item, item lv, skill, skill lv, [[item, amount]]]
        Craft craft{at(c, 0), at(c, 1), at(c, 2), at(c, 3), {}};
        for (const json::Value& m : c[size_t(4)].items()) craft.mats.emplace_back(at(m, 0), at(m, 1));
        if (craft.item > 0) ix.craft.push_back(std::move(craft));
    }
    for (const auto& f : v["flags"].members())  // {map: [flags]}
        for (const json::Value& flag : f.second.items()) ix.maps[f.first].flags.push_back(flag.asString());
    for (const json::Value& n : v["npcs"].items()) {  // [map, x, y, name, sprite] - the visible ones
        const std::string map = n[size_t(0)].asString();
        if (map.empty()) continue;
        Npc npc{int(n[size_t(1)].asInt(0)), int(n[size_t(2)].asInt(0)), n[size_t(3)].asString(),
                int(n[size_t(4)].asInt(0))};
        npc.kind = kindOf(npc.name);
        ix.maps[map].npcs.push_back(std::move(npc));
    }
    for (auto& m : ix.maps) {  // a free NPC leading to several maps: where depends on something (Eden's way back)
        std::map<std::tuple<int, int, std::string>, std::vector<std::string>> leads;
        for (const Move& mv : m.second.moves) {
            if (mv.how != How::Npc || mv.paid) continue;
            auto& to = leads[std::make_tuple(mv.x, mv.y, mv.npc)];
            if (std::find(to.begin(), to.end(), mv.to) == to.end()) to.push_back(mv.to);
        }
        for (Move& mv : m.second.moves)
            if (mv.how == How::Npc && !mv.paid && leads[std::make_tuple(mv.x, mv.y, mv.npc)].size() >= 2)
                mv.depends = true;
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
    link(ix);
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

namespace {

// How close a name is to the query (both folded): the whole of it; its start or a word's start (alike: «wing» finds Fly
// Wing, found in 185 places, before Wing Staff); anywhere
int closeness(const std::string& name, const std::string& query) {
    if (name == query) return 0;
    const std::string first = query.substr(0, query.find(' '));
    for (size_t at = name.find(first); at != std::string::npos; at = name.find(first, at + 1))
        if (at == 0 || name[at - 1] == ' ' || name[at - 1] == '_' || name[at - 1] == '-') return 1;
    return 2;
}

}  // namespace

Found search(const Index& ix, const std::string& query, size_t most) {
    Found f;
    const std::string q = trim(settings::foldCase(query));
    if (q.empty()) return f;
    struct Scored {
        int close, rank;
        std::string name;
        Hit hit;
    };
    auto finish = [&](std::vector<Scored>& list, std::vector<Hit>& hits, size_t& all) {
        std::sort(list.begin(), list.end(), [](const Scored& a, const Scored& b) {
            if (a.close != b.close) return a.close < b.close;
            if (a.rank != b.rank) return a.rank < b.rank;
            return a.name < b.name;
        });
        all = list.size();
        for (size_t i = 0; i < list.size() && i < most; ++i) hits.push_back(list[i].hit);
    };
    std::vector<Scored> mobs, items, maps;
    for (const auto& m : ix.mobs) {
        const std::string name = settings::foldCase(m.second.name);
        if (!settings::textMatches(name, q)) continue;
        const auto where = ix.spawnsOf.find(m.first);
        const int rank = (where == ix.spawnsOf.end() ? 1000 : 0) + m.second.lv;  // met somewhere, the lower first
        mobs.push_back(Scored{closeness(name, q), rank, m.second.name, Hit{Kind::Mob, m.first, ""}});
    }
    for (const auto& i : ix.items) {
        const std::string name = settings::foldCase(i.second.name);
        if (!settings::textMatches(name, q)) continue;
        const auto by = ix.droppedBy.find(i.first);
        const auto at = ix.soldAt.find(i.first);
        const int places = int((by == ix.droppedBy.end() ? 0 : by->second.size()) +
                               (at == ix.soldAt.end() ? 0 : at->second.size()));
        items.push_back(Scored{closeness(name, q), -places, i.second.name, Hit{Kind::Item, i.first, ""}});
    }
    for (const auto& m : ix.maps) {
        const auto named = ix.mapNames.find(m.first);
        const std::string name = named == ix.mapNames.end() ? "" : settings::foldCase(named->second);
        const bool byCode = settings::textMatches(m.first, q), byName = !name.empty() && settings::textMatches(name, q);
        if (!byCode && !byName) continue;
        const int close = std::min(byCode ? closeness(m.first, q) : 9, byName ? closeness(name, q) : 9);  // either
        maps.push_back(Scored{close, m.second.mobs.empty() ? 1 : 0, m.first, Hit{Kind::Map, 0, m.first}});
    }
    finish(mobs, f.mobs, f.mobsAll);
    finish(items, f.items, f.itemsAll);
    finish(maps, f.maps, f.mapsAll);
    return f;
}

std::string mapTitle(const Index& ix, const std::string& code) {
    const auto named = ix.mapNames.find(code);
    if (named == ix.mapNames.end()) return code;
    const std::string name = shortName(named->second);
    return name.empty() ? code : name + " (" + code + ")";
}

}  // namespace uagfx::refindex
