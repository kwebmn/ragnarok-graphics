#include "core/reference_pages.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <tuple>

#include "core/reference_route.hpp"

namespace uagfx::refpages {

using settings::Lang;
using settings::Text;

namespace {

constexpr size_t kMostFound = 30;  // a search's group at most (the rest: «перші 30 з 89»)

std::string say(Lang l, Text t) { return settings::tr(l, t); }
std::string sayf(Lang l, Text t, std::vector<std::string> args) { return settings::fmt(settings::tr(l, t), args); }

Row heading(const std::string& words, const std::string& right = "") {
    Row r;
    r.style = Style::Heading;
    r.left = words;
    r.right = right;
    return r;
}
Row note(const std::string& words) {
    Row r;
    r.style = Style::Note;
    r.left = words;
    return r;
}
Row linked(const std::string& left, const std::string& right, Link to) {
    Row r;
    r.left = left;
    r.right = right;
    r.link = std::move(to);
    return r;
}
Link toMob(int id) { return Link{Go::Mob, id, ""}; }
Link toItem(int id) { return Link{Go::Item, id, ""}; }
Link toMap(const std::string& code) { return Link{Go::Map, 0, code}; }

std::string mobName(const refindex::Index& ix, int id) {
    const auto it = ix.mobs.find(id);
    return it == ix.mobs.end() ? "#" + std::to_string(id) : it->second.name;
}
std::string itemName(const refindex::Index& ix, int id) {
    const auto it = ix.items.find(id);
    return it == ix.items.end() ? "#" + std::to_string(id) : it->second.name;
}
std::string minutes(Lang l, int ms) { return respawn(ms, l); }

// A monster's line on a list: «Lv 12 · prt_fild08» (its first map) - two Porings of other ids told apart
std::string mobNote(const refindex::Index& ix, Lang l, int id) {
    const auto m = ix.mobs.find(id);
    std::string s = m == ix.mobs.end() ? "" : sayf(l, Text::RefLevel, {std::to_string(m->second.lv)});
    const auto where = ix.spawnsOf.find(id);
    if (where != ix.spawnsOf.end() && !where->second.empty()) s += " · " + where->second.front().map;
    if (m != ix.mobs.end() && m->second.boss) s = say(l, Text::RefBoss) + " · " + s;
    return s;
}

// A long section's rows: all of them when opened, else the first kFew and «усі N ›» that opens it
void capped(Page& p, std::vector<Row> rows, Part part, unsigned opened, Lang l) {
    const bool all = ((opened >> unsigned(part)) & 1u) || rows.size() <= kFew + 1;
    const size_t n = all ? rows.size() : kFew;
    for (size_t i = 0; i < n; ++i) p.rows.push_back(std::move(rows[i]));
    if (!all)
        p.rows.push_back(linked(sayf(l, Text::RefAll, {std::to_string(rows.size())}), "", Link{Go::Expand, part, ""}));
}

// 3.0.11-beta.8: a move of a route - where to take it: a warp's cell, the NPC with its price
std::string noteOf(const refroute::Step& st, Lang l) {
    const refindex::Move& m = st.move;
    const std::string cell = std::to_string(m.x) + ", " + std::to_string(m.y);
    std::string s = m.how == refindex::How::Warp
                        ? sayf(l, m.to == st.from ? Text::RefRouteStairs : Text::RefRouteExit, {cell})
                        : m.npc + " " + cell;
    if (m.paid && m.price > 0) s += " · " + sayf(l, Text::RefZeny, {number(m.price, l)});
    else if (m.paid) s += " · " + say(l, Text::RefRouteVaries);
    if (m.conditional) s += " · " + say(l, Text::RefRouteCondition);
    return s;
}

void routeRows(const refindex::Index& ix, Lang l, Page& p, const refroute::Route& r, Text name, Part part,
               unsigned opened, const std::string& code, const Follow& follow) {
    std::string right = sayf(l, Text::RefRouteMoves, {std::to_string(r.steps.size())});
    if (r.zeny > 0) right += " · " + sayf(l, Text::RefZeny, {number(r.zeny, l)});
    if (r.priceUnknown) right += " · " + say(l, Text::RefRouteVaries);
    Row head;
    head.left = say(l, name);
    if (follow.can) {  // 3.0.11-beta.9: this way on the minimap («вести ›»; followed: «веду · зупинити ›»)
        const bool walk = name == Text::RefRouteWalk, followed = follow.map == code && follow.walk == walk;
        right += " · " + say(l, followed ? Text::RefRouteFollowing : Text::RefRouteFollow);
        head.link = Link{Go::Follow, walk ? 0 : 1, code};
        head.gold = followed;
    }
    head.right = right;
    head.dot = refindex::colorOf(name == Text::RefRouteWalk    ? refindex::NpcKind::Warp  // the minimap's colours
                                 : name == Text::RefRouteKafra ? refindex::NpcKind::Kafra
                                                               : refindex::NpcKind::Guide);
    p.rows.push_back(head);
    std::vector<Row> rows;
    for (size_t i = 0; i < r.steps.size(); ++i) {
        const refindex::Move& m = r.steps[i].move;
        rows.push_back(linked(std::to_string(i + 1) + ". → " + refindex::mapTitle(ix, m.to), noteOf(r.steps[i], l),
                              toMap(m.to)));
    }
    capped(p, std::move(rows), part, opened, l);
}

int conditions(const refroute::Route& r) {
    return int(std::count_if(r.steps.begin(), r.steps.end(), [](const refroute::Step& s) {
        return s.move.conditional;
    }));
}

// «Як дістатися» from the player's map: on foot (free moves) and, when paying saves moves or a condition, by Kafra
// (or an NPC); the concept's rule - both to choose, no walking for the player
void route(const refindex::Index& ix, Lang l, Page& p, const std::string& here, int hx, int hy,
           const std::string& code, unsigned opened, const Follow& follow) {
    if (!ix.graph) return;
    const refroute::Route walk = refroute::find(ix, here, hx, hy, code, true);
    const refroute::Route paid = refroute::find(ix, here, hx, hy, code, false);
    p.rows.push_back(heading(say(l, Text::RefRoute), sayf(l, Text::RefRouteFrom, {here})));
    const bool better = paid.found && paid.paid &&
                        (!walk.found || std::make_pair(conditions(paid), paid.steps.size()) <
                                            std::make_pair(conditions(walk), walk.steps.size()));
    if (!walk.found && !better) {
        p.rows.push_back(note(say(l, Text::RefRouteNone)));
        return;
    }
    if (walk.found) routeRows(ix, l, p, walk, Text::RefRouteWalk, Route1, opened, code, follow);
    if (better)
        routeRows(ix, l, p, paid, paid.kafra ? Text::RefRouteKafra : Text::RefRouteNpc, Route2, opened, code, follow);
    if ((walk.found && walk.conditional) || (better && paid.conditional))
        p.rows.push_back(note(say(l, Text::RefRouteConditionNote)));
}

std::string mapNote(const refindex::Index& ix, Lang l, const std::string& code) {
    const auto m = ix.maps.find(code);
    if (m == ix.maps.end() || m->second.mobs.empty()) return "";
    return sayf(l, Text::RefMobKinds, {std::to_string(m->second.mobs.size())});
}

}  // namespace

std::string number(long long v, Lang l) {
    const std::string digits = std::to_string(v < 0 ? -v : v);
    std::string out = v < 0 ? "-" : "";
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i && (digits.size() - i) % 3 == 0) out += l == Lang::En ? "," : " ";
        out += digits[i];
    }
    return out;
}

std::string chance(int rate, Lang l) {
    std::string s = std::to_string(rate / 100);
    if (rate % 100) {
        char frac[4];
        std::snprintf(frac, sizeof frac, "%02d", rate % 100);
        std::string f = frac;
        while (!f.empty() && f.back() == '0') f.pop_back();
        s += (l == Lang::En ? "." : ",") + f;
    }
    return s + " %";
}

std::string respawn(int ms, Lang l) {
    const int all = (ms + 30000) / 60000;
    if (all < 120) return sayf(l, Text::MinimapRespawn, {std::to_string(all)});
    if (all % 60 == 0) return sayf(l, Text::RefHours, {std::to_string(all / 60)});
    return sayf(l, Text::RefHoursMinutes, {std::to_string(all / 60), std::to_string(all % 60)});
}

std::string stepNote(const refroute::Step& step, Lang l) { return noteOf(step, l); }

std::string elementName(int e, Lang l) {
    static const Text kNames[] = {Text::RefEle0, Text::RefEle1, Text::RefEle2, Text::RefEle3, Text::RefEle4,
                                  Text::RefEle5, Text::RefEle6, Text::RefEle7, Text::RefEle8, Text::RefEle9};
    return e >= 0 && e <= 9 ? say(l, kNames[e]) : std::to_string(e);
}
std::string raceName(int r, Lang l) {
    static const Text kNames[] = {Text::RefRace0, Text::RefRace1, Text::RefRace2, Text::RefRace3, Text::RefRace4,
                                  Text::RefRace5, Text::RefRace6, Text::RefRace7, Text::RefRace8, Text::RefRace9};
    return r >= 0 && r <= 9 ? say(l, kNames[r]) : std::to_string(r);
}
std::string sizeName(int s, Lang l) {
    static const Text kNames[] = {Text::RefSize0, Text::RefSize1, Text::RefSize2};
    return s >= 0 && s <= 2 ? say(l, kNames[s]) : std::to_string(s);
}
std::string typeName(int t, Lang l) {
    switch (t) {
        case 0: return say(l, Text::RefType0);
        case 2: return say(l, Text::RefType2);
        case 3: return say(l, Text::RefType3);
        case 4: return say(l, Text::RefType4);
        case 5: return say(l, Text::RefType5);
        case 6: return say(l, Text::RefType6);
        case 7: return say(l, Text::RefType7);
        case 8: return say(l, Text::RefType8);
        case 10: return say(l, Text::RefType10);
        case 11: return say(l, Text::RefType11);
        case 18: return say(l, Text::RefType18);
        default: return say(l, Text::RefTypeOther);
    }
}

Page home(const refindex::Index& ix, Lang l, int online) {
    Page p;
    p.name = say(l, Text::RefTitle);
    size_t bosses = 0, cards = 0, maps = 0, kafra = 0;
    for (const auto& m : ix.mobs)
        if (m.second.boss && ix.spawnsOf.count(m.first)) ++bosses;
    for (const auto& i : ix.items)
        if (i.second.type == 6) ++cards;
    for (const auto& m : ix.maps) {
        if (!m.second.mobs.empty()) ++maps;
        for (const refindex::Teleport& t : m.second.teleports)
            if (t.kafra) {
                ++kafra;
                break;
            }
    }
    const std::tuple<Section, Text, Text, size_t> tiles[] = {
        {Mobs, Text::RefSecMobs, Text::RefSecMobsHint, ix.mobs.size()},
        {Bosses, Text::RefSecBosses, Text::RefSecBossesHint, bosses},
        {Items, Text::RefSecItems, Text::RefSecItemsHint, ix.items.size()},
        {Cards, Text::RefSecCards, Text::RefSecCardsHint, cards},
        {Maps, Text::RefSecMaps, Text::RefSecMapsHint, maps},
        {Shops, Text::RefSecShops, Text::RefSecShopsHint, ix.shops.size()},
        {Teleports, Text::RefSecTeleports, Text::RefSecTeleportsHint, kafra},
        {Pets, Text::RefSecPets, Text::RefSecPetsHint, ix.pets.size()},
        {Crafting, Text::RefSecCraft, Text::RefSecCraftHint, ix.craft.size()},
    };
    for (const auto& [id, name, hint, count] : tiles) {
        Row r;
        r.style = Style::Tile;
        r.left = say(l, name) + " · " + number(static_cast<long long>(count), l);
        r.right = say(l, hint);
        r.link = Link{Go::Section, int(id), ""};
        p.rows.push_back(std::move(r));
    }
    Row world;  // 3.0.11-beta.11: the tenth - the world map
    world.style = Style::Tile;
    world.left = say(l, Text::RefWorld);
    world.right = say(l, Text::RefWorldHint);
    world.link = Link{Go::World, 0, ""};
    p.rows.push_back(std::move(world));
    if (online > 0) p.rows.push_back(note(sayf(l, Text::RefOnlineNow, {number(online, l)})));  // 3.0.11-beta.10
    p.rows.push_back(note(say(l, Text::RefKeys)));
    return p;
}

Page found(const refindex::Index& ix, Lang l, const std::string& query) {
    Page p;
    p.name = "«" + query + "»";
    const refindex::Found f = refindex::search(ix, query, kMostFound);
    const auto shown = [&](size_t n, size_t all) {
        if (all > n) p.rows.push_back(note(sayf(l, Text::RefShownOf, {std::to_string(n), std::to_string(all)})));
    };
    if (!f.mobs.empty()) {
        p.rows.push_back(heading(sayf(l, Text::RefFoundMobs, {std::to_string(f.mobsAll)})));
        for (const refindex::Hit& h : f.mobs) {
            Row r = linked(mobName(ix, h.id), mobNote(ix, l, h.id), toMob(h.id));
            r.gold = ix.mobs.at(h.id).boss;
            p.rows.push_back(std::move(r));
        }
        shown(f.mobs.size(), f.mobsAll);
    }
    if (!f.items.empty()) {
        p.rows.push_back(heading(sayf(l, Text::RefFoundItems, {std::to_string(f.itemsAll)})));
        for (const refindex::Hit& h : f.items)
            p.rows.push_back(linked(itemName(ix, h.id), typeName(ix.items.at(h.id).type, l), toItem(h.id)));
        shown(f.items.size(), f.itemsAll);
    }
    if (!f.maps.empty()) {
        p.rows.push_back(heading(sayf(l, Text::RefFoundMaps, {std::to_string(f.mapsAll)})));
        for (const refindex::Hit& h : f.maps)
            p.rows.push_back(linked(refindex::mapTitle(ix, h.map), mapNote(ix, l, h.map), toMap(h.map)));
        shown(f.maps.size(), f.mapsAll);
    }
    if (p.rows.empty()) p.rows.push_back(note(say(l, Text::RefNothing)));
    return p;
}

Page section(const refindex::Index& ix, Lang l, int id) {
    Page p;
    if (id >= 100) {  // the items of one type, by name
        p.name = typeName(id - 100, l);
        p.rows.push_back(heading(p.name));
        std::vector<std::pair<std::string, int>> list;
        for (const auto& i : ix.items)
            if (i.second.type == id - 100) list.emplace_back(i.second.name, i.first);
        std::sort(list.begin(), list.end());
        for (const auto& [name, item] : list) {
            const auto by = ix.droppedBy.find(item);
            const auto at = ix.soldAt.find(item);
            std::string right;
            if (at != ix.soldAt.end()) right = sayf(l, Text::RefZeny, {number(at->second.front().second, l)});
            else if (by != ix.droppedBy.end()) right = mobName(ix, by->second.front().mob);
            p.rows.push_back(linked(name, right, toItem(item)));
        }
        return p;
    }
    switch (id) {
        case Mobs: {
            p.name = say(l, Text::RefSecMobs);
            p.rows.push_back(heading(p.name));
            std::vector<std::tuple<int, std::string, int>> list;  // level, name, id
            for (const auto& m : ix.mobs) list.emplace_back(m.second.lv, m.second.name, m.first);
            std::sort(list.begin(), list.end());
            for (const auto& [lv, name, mob] : list) {
                Row r = linked(name, mobNote(ix, l, mob), toMob(mob));
                r.gold = ix.mobs.at(mob).boss;
                p.rows.push_back(std::move(r));
            }
            break;
        }
        case Bosses: {
            p.name = say(l, Text::RefSecBosses);
            p.rows.push_back(heading(p.name));
            std::vector<std::tuple<int, std::string, int>> list;  // the longest respawn first
            for (const auto& m : ix.mobs) {
                const auto where = ix.spawnsOf.find(m.first);
                if (!m.second.boss || where == ix.spawnsOf.end()) continue;
                int longest = 0;
                for (const refindex::Where& w : where->second) longest = std::max(longest, w.respawnMs);
                list.emplace_back(-longest, m.second.name, m.first);
            }
            std::sort(list.begin(), list.end());
            for (const auto& [neg, name, mob] : list) {
                const refindex::Where& w = ix.spawnsOf.at(mob).front();
                std::string right = refindex::mapTitle(ix, w.map);
                if (-neg >= 600000) right += " · " + minutes(l, -neg);
                Row r = linked(name, right, toMob(mob));
                r.gold = true;
                p.rows.push_back(std::move(r));
            }
            break;
        }
        case Items: {  // the types, each with its count
            p.name = say(l, Text::RefSecItems);
            p.rows.push_back(heading(p.name));
            std::map<int, size_t> types;
            for (const auto& i : ix.items) ++types[i.second.type];
            for (const auto& [type, count] : types)
                p.rows.push_back(linked(typeName(type, l), number(static_cast<long long>(count), l),
                                        Link{Go::Section, 100 + type, ""}));
            break;
        }
        case Cards: {
            p.name = say(l, Text::RefSecCards);
            p.rows.push_back(heading(p.name));
            std::vector<std::pair<std::string, int>> list;
            for (const auto& i : ix.items)
                if (i.second.type == 6) list.emplace_back(i.second.name, i.first);
            std::sort(list.begin(), list.end());
            for (const auto& [name, card] : list) {
                const auto by = ix.droppedBy.find(card);
                const std::string right = by == ix.droppedBy.end()
                                              ? ""
                                              : mobName(ix, by->second.front().mob) + " · " +
                                                    chance(by->second.front().rate, l);
                p.rows.push_back(linked(name, right, toItem(card)));
            }
            break;
        }
        case Maps: {
            p.name = say(l, Text::RefSecMaps);
            p.rows.push_back(heading(p.name));
            std::vector<std::pair<std::string, std::string>> list;  // the title, the code
            for (const auto& m : ix.maps)
                if (!m.second.mobs.empty()) list.emplace_back(refindex::mapTitle(ix, m.first), m.first);
            std::sort(list.begin(), list.end());
            for (const auto& [title, code] : list) p.rows.push_back(linked(title, mapNote(ix, l, code), toMap(code)));
            break;
        }
        case Shops: {
            p.name = say(l, Text::RefSecShops);
            p.rows.push_back(heading(p.name));
            std::vector<std::tuple<std::string, std::string, int>> list;  // the map's title, the shop, its index
            for (size_t s = 0; s < ix.shops.size(); ++s)
                list.emplace_back(refindex::mapTitle(ix, ix.shops[s].map), ix.shops[s].name, int(s));
            std::sort(list.begin(), list.end());
            for (const auto& [title, name, s] : list) {
                const std::string goods = std::to_string(ix.shops[size_t(s)].items.size());
                p.rows.push_back(
                    linked(name + " · " + title, sayf(l, Text::RefItemsCount, {goods}), Link{Go::Shop, s, ""}));
            }
            break;
        }
        case Teleports: {
            p.name = say(l, Text::RefSecTeleports);
            p.rows.push_back(heading(p.name));
            for (const auto& m : ix.maps) {
                std::set<std::string> dests;
                for (const refindex::Teleport& t : m.second.teleports)
                    if (t.kafra) dests.insert(t.dest);
                if (dests.empty()) continue;
                std::string right;
                for (const std::string& d : dests) {
                    const auto named = ix.mapNames.find(d);
                    const std::string name = named == ix.mapNames.end() ? d : refindex::shortName(named->second);
                    right += (right.empty() ? "" : ", ") + name;
                }
                p.rows.push_back(linked(refindex::mapTitle(ix, m.first), right, toMap(m.first)));
            }
            std::sort(p.rows.begin() + 1, p.rows.end(), [](const Row& a, const Row& b) { return a.left < b.left; });
            break;
        }
        case Pets: {
            p.name = say(l, Text::RefSecPets);
            p.rows.push_back(heading(p.name));
            for (const refindex::Pet& pet : ix.pets)
                p.rows.push_back(linked(mobName(ix, pet.mob), say(l, Text::RefPetLure) + ": " + itemName(ix, pet.lure),
                                        toMob(pet.mob)));
            std::sort(p.rows.begin() + 1, p.rows.end(), [](const Row& a, const Row& b) { return a.left < b.left; });
            break;
        }
        case Crafting: {
            p.name = say(l, Text::RefSecCraft);
            p.rows.push_back(heading(p.name));
            for (const refindex::Craft& c : ix.craft) {
                std::string mats;
                for (const auto& [mat, n] : c.mats)
                    mats += (mats.empty() ? "" : ", ") + itemName(ix, mat) + " ×" + std::to_string(n);
                p.rows.push_back(linked(itemName(ix, c.item), mats, toItem(c.item)));
            }
            std::sort(p.rows.begin() + 1, p.rows.end(), [](const Row& a, const Row& b) { return a.left < b.left; });
            break;
        }
        default:
            break;
    }
    return p;
}

Page mob(const refindex::Index& ix, Lang l, int id, const std::string& here, unsigned opened) {
    Page p;
    const auto it = ix.mobs.find(id);
    if (it == ix.mobs.end()) return p;
    const refindex::Mob& m = it->second;
    p.name = m.name;
    Row title;
    title.style = Style::Title;
    title.left = m.name;
    title.right = sayf(l, Text::RefLevel, {std::to_string(m.lv)});
    title.gold = m.boss;
    p.rows.push_back(title);
    Row sub;
    sub.style = Style::Sub;
    sub.left = sayf(l, Text::RefHp, {number(m.hp, l)}) + " · " + elementName(m.element, l) + " " +
               std::to_string(m.elementLv) + " · " + raceName(m.race, l) + " · " + sizeName(m.size, l) + " · " +
               sayf(l, Text::RefExp, {number(m.baseExp, l), number(m.jobExp, l)});
    if (m.boss) sub.left += " · ★ " + say(l, Text::RefBoss);
    p.rows.push_back(sub);

    const auto where = ix.spawnsOf.find(id);
    p.rows.push_back(heading(say(l, Text::RefWhere)));
    if (where == ix.spawnsOf.end() || where->second.empty()) {
        p.rows.push_back(note(say(l, Text::RefNowhere)));
    } else {
        std::vector<refindex::Where> list = where->second;
        std::stable_partition(list.begin(), list.end(), [&](const refindex::Where& w) { return w.map == here; });
        std::vector<Row> rows;
        for (const refindex::Where& w : list) {
            std::string right = "×" + std::to_string(w.count);
            if (w.respawnMs >= 600000) right += " · " + minutes(l, w.respawnMs);
            if (w.map == here) right = say(l, Text::RefYouHere) + " · " + right;
            Row r = linked(refindex::mapTitle(ix, w.map), right, toMap(w.map));
            r.gold = w.map == here;
            rows.push_back(std::move(r));
        }
        p.rows.back().right = std::to_string(list.size());
        capped(p, std::move(rows), Where, opened, l);
    }
    p.rows.push_back(heading(say(l, Text::RefDrops)));
    if (m.drops.empty()) p.rows.push_back(note(say(l, Text::RefNoDrops)));
    std::vector<refindex::Drop> drops = m.drops;
    std::stable_sort(drops.begin(), drops.end(), [](const refindex::Drop& a, const refindex::Drop& b) {
        return a.kind != b.kind ? a.kind < b.kind : a.rate > b.rate;
    });
    std::vector<Row> dropRows;
    for (const refindex::Drop& d : drops) {
        std::string right = chance(d.rate, l);
        if (d.kind == 1) right = say(l, Text::RefMvpReward) + " · " + right;
        dropRows.push_back(linked(itemName(ix, d.item), right, toItem(d.item)));
    }
    capped(p, std::move(dropRows), Drops, opened, l);
    for (const refindex::Pet& pet : ix.pets) {  // a pet: what tames it, its egg, its food, its accessory
        if (pet.mob != id) continue;
        p.rows.push_back(heading(say(l, Text::RefPet)));
        const std::pair<int, Text> parts[] = {{pet.lure, Text::RefPetLure}, {pet.egg, Text::RefPetEgg},
                                              {pet.food, Text::RefPetFood}, {pet.equip, Text::RefPetEquip}};
        for (const auto& [item, what] : parts)
            if (item > 0) p.rows.push_back(linked(itemName(ix, item), say(l, what), toItem(item)));
    }
    return p;
}

Page item(const refindex::Index& ix, Lang l, int id, unsigned opened) {
    Page p;
    const auto it = ix.items.find(id);
    if (it == ix.items.end()) return p;
    const refindex::Item& i = it->second;
    p.name = i.name;
    Row title;
    title.style = Style::Title;
    title.left = i.name + (i.slots > 0 ? " [" + std::to_string(i.slots) + "]" : "");
    title.right = typeName(i.type, l);
    p.rows.push_back(title);
    Row sub;
    sub.style = Style::Sub;
    sub.left = sayf(l, Text::RefWeight, {number(i.weight / 10, l) + (i.weight % 10 ? (l == Lang::En ? "." : ",") +
                                                                                       std::to_string(i.weight % 10)
                                                                                 : "")});
    if (i.buy > 0) sub.left += " · " + sayf(l, Text::RefPrice, {number(i.buy, l)});
    if (i.slots > 0) sub.left += " · " + sayf(l, Text::RefSlots, {std::to_string(i.slots)});
    p.rows.push_back(sub);

    const auto by = ix.droppedBy.find(id);
    if (by != ix.droppedBy.end()) {
        p.rows.push_back(heading(say(l, Text::RefDroppedBy), std::to_string(by->second.size())));
        std::vector<Row> rows;
        for (const refindex::Source& s : by->second) {
            std::string right = chance(s.rate, l);
            if (s.kind == 1) right = say(l, Text::RefMvpReward) + " · " + right;
            const std::string note = mobNote(ix, l, s.mob);
            Row r = linked(mobName(ix, s.mob), right + (note.empty() ? "" : " · " + note), toMob(s.mob));
            r.gold = ix.mobs.count(s.mob) && ix.mobs.at(s.mob).boss;
            rows.push_back(std::move(r));
        }
        capped(p, std::move(rows), DroppedBy, opened, l);
    }
    const auto at = ix.soldAt.find(id);
    if (at != ix.soldAt.end()) {
        p.rows.push_back(heading(say(l, Text::RefSoldAt), std::to_string(at->second.size())));
        std::vector<Row> rows;
        for (const auto& [s, price] : at->second) {
            const refindex::Shop& shop = ix.shops[size_t(s)];
            rows.push_back(linked(shop.name + " · " + refindex::mapTitle(ix, shop.map),
                                  sayf(l, Text::RefZeny, {number(price, l)}), Link{Go::Shop, s, ""}));
        }
        capped(p, std::move(rows), SoldAt, opened, l);
    }
    const auto made = ix.craftOf.find(id);
    if (made != ix.craftOf.end()) {
        p.rows.push_back(heading(say(l, Text::RefCraftOf)));
        for (int c : made->second)
            for (const auto& [mat, n] : ix.craft[size_t(c)].mats)
                p.rows.push_back(linked(itemName(ix, mat), "×" + std::to_string(n), toItem(mat)));
    }
    const auto used = ix.usedIn.find(id);
    if (used != ix.usedIn.end()) {
        p.rows.push_back(heading(say(l, Text::RefUsedIn), std::to_string(used->second.size())));
        std::vector<Row> rows;
        for (int c : used->second) {
            const int made = ix.craft[size_t(c)].item;
            rows.push_back(linked(itemName(ix, made), "", toItem(made)));
        }
        capped(p, std::move(rows), UsedIn, opened, l);
    }
    for (const refindex::Pet& pet : ix.pets) {
        const Text what = pet.lure == id ? Text::RefPetLure : pet.egg == id ? Text::RefPetEgg
                        : pet.food == id ? Text::RefPetFood : pet.equip == id ? Text::RefPetEquip : Text::Count;
        if (what == Text::Count) continue;
        p.rows.push_back(heading(say(l, Text::RefPet)));
        p.rows.push_back(linked(mobName(ix, pet.mob), say(l, what), toMob(pet.mob)));
    }
    if (p.rows.size() == 2) p.rows.push_back(note(say(l, Text::RefNowhere)));
    return p;
}

Page map(const refindex::Index& ix, Lang l, const std::string& code, const Context& c, unsigned opened) {
    const std::string& here = c.here;
    Page p;
    p.name = refindex::mapTitle(ix, code);
    const auto named = ix.mapNames.find(code);
    Row title;
    title.style = Style::Title;
    title.left = named == ix.mapNames.end() ? code : refindex::shortName(named->second);
    title.right = code;
    title.gold = code == here;
    p.rows.push_back(title);
    const auto it = ix.maps.find(code);
    Row sub;
    sub.style = Style::Sub;
    if (named != ix.mapNames.end() && refindex::shortName(named->second) != named->second) sub.left = named->second;
    if (it != ix.maps.end()) {
        const std::pair<const char*, Text> flags[] = {
            {"town", Text::RefFlagTown},         {"pvp", Text::RefFlagPvp},        {"gvg", Text::RefFlagGvg},
            {"noteleport", Text::RefFlagNoTeleport}, {"nomemo", Text::RefFlagNoMemo}, {"nosave", Text::RefFlagNoSave}};
        for (const auto& [flag, words] : flags)
            if (std::find(it->second.flags.begin(), it->second.flags.end(), flag) != it->second.flags.end())
                sub.left += (sub.left.empty() ? "" : " · ") + say(l, words);
    }
    if (code == here) sub.left += (sub.left.empty() ? "" : " · ") + say(l, Text::RefYouHere);
    if (c.players) {  // 3.0.11-beta.10: the site's «Онлайн» now
        const auto now = c.players->find(code);
        if (now != c.players->end() && now->second > 0)
            sub.left += (sub.left.empty() ? "" : " · ") + sayf(l, Text::RefPlayersNow, {std::to_string(now->second)});
    }
    if (!sub.left.empty()) p.rows.push_back(sub);
    if (!here.empty() && here != code) route(ix, l, p, here, c.x, c.y, code, opened, c.follow);
    if (it == ix.maps.end()) return p;
    const refindex::MapHints& h = it->second;
    if (!h.mobs.empty()) {
        p.rows.push_back(heading(say(l, Text::RefWhoHere), std::to_string(h.mobs.size())));
        std::vector<Row> rows;
        for (const refindex::MobOnMap& o : h.mobs) {
            const auto m = ix.mobs.find(o.id);
            std::string right = "×" + std::to_string(o.count);
            if (m != ix.mobs.end()) right += " · " + sayf(l, Text::RefLevel, {std::to_string(m->second.lv)});
            if (m != ix.mobs.end() && m->second.boss && o.respawnMs >= 600000)
                right += " · " + minutes(l, o.respawnMs);
            Row r = linked(mobName(ix, o.id), right, toMob(o.id));
            r.gold = m != ix.mobs.end() && m->second.boss;
            rows.push_back(std::move(r));
        }
        capped(p, std::move(rows), WhoHere, opened, l);
    }
    std::vector<std::string> exits;
    for (const refindex::Warp& w : h.warps)
        if (std::find(exits.begin(), exits.end(), w.dest) == exits.end()) exits.push_back(w.dest);
    if (!exits.empty()) {
        p.rows.push_back(heading(say(l, Text::RefExits), std::to_string(exits.size())));
        std::vector<Row> rows;
        for (const std::string& d : exits)
            rows.push_back(linked("→ " + refindex::mapTitle(ix, d), mapNote(ix, l, d), toMap(d)));
        capped(p, std::move(rows), Exits, opened, l);
    }
    std::vector<std::tuple<std::string, std::string, int>> teleports;  // who, where, the price - once each
    for (const refindex::Teleport& t : h.teleports)
        if (std::find(teleports.begin(), teleports.end(), std::make_tuple(t.npc, t.dest, t.price)) == teleports.end())
            teleports.emplace_back(t.npc, t.dest, t.price);
    if (!teleports.empty()) {
        p.rows.push_back(heading(say(l, Text::RefTeleports), std::to_string(teleports.size())));
        std::vector<Row> rows;
        for (const auto& [npc, dest, price] : teleports)
            rows.push_back(linked(npc + " → " + refindex::mapTitle(ix, dest),
                                  price >= 0 ? sayf(l, Text::RefZeny, {number(price, l)}) : "", toMap(dest)));
        capped(p, std::move(rows), Teleports2, opened, l);
    }
    std::vector<int> shops;
    for (size_t s = 0; s < ix.shops.size(); ++s)
        if (ix.shops[s].map == code) shops.push_back(int(s));
    if (!shops.empty()) {
        p.rows.push_back(heading(say(l, Text::RefShopsHere), std::to_string(shops.size())));
        std::vector<Row> rows;
        for (int s : shops)
            rows.push_back(linked(ix.shops[size_t(s)].name,
                                  sayf(l, Text::RefItemsCount, {std::to_string(ix.shops[size_t(s)].items.size())}),
                                  Link{Go::Shop, s, ""}));
        capped(p, std::move(rows), ShopsHere, opened, l);
    }
    if (!h.npcs.empty()) {
        p.rows.push_back(heading(say(l, Text::RefNpcs), std::to_string(h.npcs.size())));
        std::vector<Row> rows;
        for (const refindex::Npc& n : h.npcs) {
            Row r;
            r.left = n.name;
            r.right = std::to_string(n.x) + ", " + std::to_string(n.y);
            r.dot = refindex::colorOf(n.kind);
            rows.push_back(std::move(r));
        }
        capped(p, std::move(rows), Npcs, opened, l);
    }
    return p;
}

Page shop(const refindex::Index& ix, Lang l, int index) {
    Page p;
    if (index < 0 || size_t(index) >= ix.shops.size()) return p;
    const refindex::Shop& s = ix.shops[size_t(index)];
    p.name = s.name;
    Row title;
    title.style = Style::Title;
    title.left = s.name;
    title.right = std::to_string(s.x) + ", " + std::to_string(s.y);
    p.rows.push_back(title);
    p.rows.push_back(linked(refindex::mapTitle(ix, s.map), "", toMap(s.map)));
    p.rows.push_back(heading(say(l, Text::RefShopItems), std::to_string(s.items.size())));
    for (const auto& [it, price] : s.items)
        p.rows.push_back(linked(itemName(ix, it), sayf(l, Text::RefZeny, {number(price, l)}), toItem(it)));
    return p;
}

namespace {

// A map's line on the world's lists: its monsters' kinds and its players now
std::string mapLine(const refindex::Index& ix, Lang l, const std::string& code, const Context& c) {
    std::string s = mapNote(ix, l, code);
    if (c.players) {
        const auto now = c.players->find(code);
        if (now != c.players->end() && now->second > 0)
            s += (s.empty() ? "" : " · ") + sayf(l, Text::RefPlayersNow, {std::to_string(now->second)});
    }
    return s;
}

}  // namespace

Page world(const refindex::Index& ix, Lang l, const Context& c) {
    Page p;
    p.name = say(l, Text::RefWorld);
    Row title;
    title.style = Style::Title;
    title.left = say(l, Text::RefWorld);
    title.right = say(l, Text::RefDungeons) + " ›";
    title.link = Link{Go::World, 1, ""};
    p.rows.push_back(title);
    if (!c.world) {
        p.rows.push_back(note(say(l, Text::RefWorldNone)));
        return p;
    }
    Row picture;
    picture.style = Style::World;
    p.rows.push_back(picture);
    const worldmap::Tile* t = c.hover.empty() ? nullptr : worldmap::tileOf(*c.world, c.hover);
    if (!t) {
        p.rows.push_back(note(say(l, Text::RefWorldHelp)));
        return p;
    }
    std::string line = refindex::mapTitle(ix, t->map);
    const std::string more = mapLine(ix, l, t->map, c);
    if (!more.empty()) line += " · " + more;
    if (t->map == c.here) line += " · " + say(l, Text::RefYouHere);
    p.rows.push_back(note(line));
    return p;
}

Page dungeons(const refindex::Index& ix, Lang l, const Context& c) {
    Page p;
    p.name = say(l, Text::RefDungeons);
    Row title;
    title.style = Style::Title;
    title.left = say(l, Text::RefDungeons);
    title.right = say(l, Text::RefWorld) + " ›";
    title.link = Link{Go::World, 0, ""};
    p.rows.push_back(title);
    if (!c.world) {
        p.rows.push_back(note(say(l, Text::RefWorldNone)));
        return p;
    }
    std::string group;
    for (const worldmap::Dungeon& d : c.world->dungeons) {
        if (d.group != group || p.rows.size() == 1) {
            group = d.group;
            p.rows.push_back(heading(group.empty() ? d.map : group));
        }
        Row r = linked(refindex::mapTitle(ix, d.map), mapLine(ix, l, d.map, c), toMap(d.map));
        r.gold = d.map == c.here;
        p.rows.push_back(std::move(r));
    }
    return p;
}

Page open(const refindex::Index& ix, Lang l, const Link& link, const Context& c, unsigned opened) {
    switch (link.go) {
        case Go::Mob: return mob(ix, l, link.id, c.here, opened);
        case Go::Item: return item(ix, l, link.id, opened);
        case Go::Map: return map(ix, l, link.map, c, opened);
        case Go::Shop: return shop(ix, l, link.id);
        case Go::Section: return section(ix, l, link.id);
        case Go::World: return link.id == 1 ? dungeons(ix, l, c) : world(ix, l, c);
        case Go::None:
        case Go::Expand:
        case Go::Follow: break;
    }
    return home(ix, l);
}

}  // namespace uagfx::refpages
