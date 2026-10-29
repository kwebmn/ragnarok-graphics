#include "core/reference_pages.hpp"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>

#include "core/reference_route.hpp"
#include "core/settings/search.hpp"

namespace uagfx::refpages {

using settings::Lang;
using settings::Text;

namespace {
#include "core/attr_fix.inc"  // 3.0.11-beta.13: kAttrFix, tools/attr_fix.py from the server's db/attr_fix.txt
}  // namespace

namespace {

constexpr size_t kMostFound = 100;  // a search's group at most (3.0.11-beta.28: opened; «перші 100 з 340»)

std::string say(Lang l, Text t) { return settings::tr(l, t); }
std::string sayf(Lang l, Text t, std::vector<std::string> args) { return settings::fmt(settings::tr(l, t), args); }

Row heading(const std::string& words, const std::string& right = "", int fold = -1) {
    Row r;
    r.style = Style::Heading;
    r.left = words;
    r.right = right;
    if (fold >= 0) {  // 3.0.11-beta.17: a click folds it (in every card)
        r.fold = fold;
        r.link = Link{Go::Fold, fold, ""};
    }
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
    if (to.go == Go::Item) r.icon = "item:" + std::to_string(to.id);  // 3.0.11-beta.15: its icon before it
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

// 3.0.11-beta.16: the quests' language of the interface's, a quest's kind in words
int questLang(Lang l) { return l == Lang::Ru ? refquests::Ru : l == Lang::En ? refquests::En : refquests::Uk; }

std::string categoryName(const std::string& c, Lang l) {
    if (c == "jobchange") return say(l, Text::RefQuestJobs);
    if (c == "headgear") return say(l, Text::RefQuestHeadgear);
    if (c == "access") return say(l, Text::RefQuestAccess);
    if (c == "event") return say(l, Text::RefQuestEvents);
    return say(l, Text::RefQuestOther);
}

Row text(const std::string& words) {
    Row r;
    r.style = Style::Text;
    r.left = words;
    return r;
}

Link toQuest(int index) { return Link{Go::Quest, index, ""}; }

// 3.0.11-beta.12: a time as the respawns are told («23 хв», «2 год 5 хв»), a minute at the least
std::string respawnText(long long seconds, Lang l) {
    const long long ms = std::max<long long>(seconds, 60) * 1000;
    return respawn(int(std::min<long long>(ms, INT_MAX)), l);  // up to some 596 hours
}

// 3.0.11-beta.12: a monster's respawn on a map after death, as the pack has it (the map not told: its first), 0 none
long long respawnOn(const refindex::Index& ix, int mob, const std::string& map) {
    const auto where = ix.spawnsOf.find(mob);
    if (where == ix.spawnsOf.end() || where->second.empty()) return 0;
    for (const refindex::Where& w : where->second)
        if (w.map == map) return w.respawnMs;
    return map.empty() ? where->second.front().respawnMs : 0;
}

// «Таймери гравців»: the public timers of the site's MVP page (the latest a map) - when it comes back by the server's
// respawn, how long ago it was killed
void timers(const refindex::Index& ix, Lang l, Page& p, int mob, const Context& c) {
    if (!c.live || c.now <= 0) return;
    std::vector<const reflive::Timer*> latest;
    for (const reflive::Timer& t : c.live->timers)
        if (t.mob == mob && std::none_of(latest.begin(), latest.end(), [&](const reflive::Timer* o) {
                return o->map == t.map;
            }))
            latest.push_back(&t);
    if (latest.empty()) return;
    p.rows.push_back(heading(say(l, Text::RefTimers), std::to_string(latest.size()), Timers));
    for (const reflive::Timer* t : latest) {
        const long long ago = std::max(0LL, c.now - t->killed);
        const long long respawn = respawnOn(ix, mob, t->map) / 1000;
        std::string right = sayf(l, Text::RefTimerAgo, {respawnText(ago, l)});
        if (respawn > 0) {
            const long long left = t->killed + respawn - c.now;
            right = (left > 0 ? sayf(l, Text::RefTimerSoon, {respawnText(left, l)}) : say(l, Text::RefTimerDue)) +
                    " · " + right;
        }
        Row r = t->map.empty() ? linked(say(l, Text::RefTimerNoMap), right, Link{})
                               : linked(refindex::mapTitle(ix, t->map), right, toMap(t->map));
        r.gold = respawn > 0 && t->killed + respawn <= c.now;
        p.rows.push_back(std::move(r));
    }
}

// «Шкода стихіями»: how much of each element's attack the monster takes (the server's table), the most first - the
// skills and the weapons to take on it; below 0 the attack heals it (attribute_recover)
void elements(Lang l, Page& p, const refindex::Mob& m) {
    std::vector<std::pair<int, int>> by;  // element, per cent
    for (int e = 0; e < 10; ++e) by.emplace_back(e, elementDamage(e, m.element, m.elementLv));
    std::stable_sort(by.begin(), by.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    // 3.0.11-beta.26: the concept's - its line says the most and what heals; its elements as chips, each its dot
    std::string most, heals;
    for (const auto& [e, pct] : by) {
        if (by.front().second > 100 && pct == by.front().second) most += (most.empty() ? "" : ", ") + elementName(e, l);
        if (pct < 0) heals += (heals.empty() ? "" : ", ") + elementName(e, l);
    }
    std::string line = elementName(m.element, l) + " " + std::to_string(m.elementLv);
    if (!most.empty()) line += " · " + most + " " + std::to_string(by.front().second) + " %";
    if (!heals.empty()) line += " · " + sayf(l, Text::RefElementsHeal, {heals});
    p.rows.push_back(heading(say(l, Text::RefElements), line, Elements));
    for (const auto& [e, pct] : by) {
        Row r;
        r.style = Style::Chip;
        r.left = elementName(e, l);
        r.right = pct < 0 ? sayf(l, Text::RefElementHeals, {std::to_string(-pct)}) : std::to_string(pct) + " %";
        r.tone = pct < 0 ? -1 : pct > 100 ? 1 : 0;
        r.dot = elementColour(e);
        p.rows.push_back(std::move(r));
    }
}

// «На ринку сайту»: the cheapest of the active lots, how many lots and pieces
void market(Lang l, Page& p, int item, const Context& c) {
    if (!c.live) return;
    const auto m = c.live->market.find(item);
    if (m == c.live->market.end()) return;
    p.rows.push_back(heading(say(l, Text::RefMarket), "", Market));
    Row r;
    r.left = sayf(l, Text::RefMarketFrom, {sayf(l, Text::RefZeny, {number(m->second.cheapest, l)})});
    r.right = sayf(l, Text::RefMarketLots, {std::to_string(m->second.lots), std::to_string(m->second.pieces)});
    r.gold = true;
    p.rows.push_back(std::move(r));
}

// 3.0.11-beta.12: how far a map is - «переходів: 3», by paying «переходів: 1 · Kafra»
std::string farNote(const refroute::Far& f, Lang l) {
    std::string s = sayf(l, Text::RefRouteMoves, {std::to_string(f.moves)});
    if (f.paid) s += f.kafra ? " · Kafra" : " · NPC";
    return s;
}

// 3.0.11-beta.15: «Kafra поруч» - the Kafra nearest to a map (on it, or the fewest moves away): storage, save,
// teleports
void kafraNear(const refindex::Index& ix, Lang l, Page& p, const std::string& code) {
    auto kafraOn = [&](const std::string& map) -> const refindex::Npc* {
        const auto it = ix.maps.find(map);
        if (it == ix.maps.end()) return nullptr;
        for (const refindex::Npc& n : it->second.npcs)
            if (n.kind == refindex::NpcKind::Kafra) return &n;
        return nullptr;
    };
    Row r;
    if (const refindex::Npc* here = kafraOn(code)) {
        p.rows.push_back(heading(say(l, Text::RefKafraNear), "", Kafra));
        r.left = here->name;
        r.right = say(l, Text::RefHere) + " · " + std::to_string(here->x) + ", " + std::to_string(here->y);
        r.dot = refindex::colorOf(here->kind);
        p.rows.push_back(std::move(r));
        return;
    }
    const auto moves = refroute::movesFrom(ix, code);
    std::string best;
    const refindex::Npc* npc = nullptr;
    int bestKey = INT_MAX;
    for (const auto& [map, far] : moves) {
        const refindex::Npc* n = kafraOn(map);
        const int key = far.moves * 2 + (far.paid ? 1 : 0);
        if (n && (key < bestKey || (key == bestKey && map < best))) {
            bestKey = key;
            best = map;
            npc = n;
        }
    }
    if (!npc) return;
    p.rows.push_back(heading(say(l, Text::RefKafraNear), "", Kafra));
    r = linked(refindex::mapTitle(ix, best),
               farNote(moves.at(best), l) + " · " + std::to_string(npc->x) + ", " + std::to_string(npc->y),
               toMap(best));
    r.dot = refindex::colorOf(npc->kind);
    p.rows.push_back(std::move(r));
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

// A way's line: its name, its moves and price, «вести ›» (3.0.11-beta.26: in the main block; its steps
// under it apart)
void routeHead(Lang l, Page& p, const refroute::Route& r, Text name, const std::string& code, const Follow& follow) {
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
}

// 3.0.11-beta.26: a way's steps - a section of the card that folds, after its main blocks
void routeSteps(const refindex::Index& ix, Lang l, Page& p, const refroute::Route& r, Text name, Part part,
                unsigned opened) {
    p.rows.push_back(heading(sayf(l, Text::RefRouteSteps, {say(l, name)}),
                             sayf(l, Text::RefRouteMoves, {std::to_string(r.steps.size())}), part));
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
    p.rows.push_back(heading(say(l, Text::RefRoute), sayf(l, Text::RefRouteFrom, {here}), Way));
    const bool better = paid.found && paid.paid &&
                        (!walk.found || std::make_pair(conditions(paid), paid.steps.size()) <
                                            std::make_pair(conditions(walk), walk.steps.size()));
    if (!walk.found && !better) {
        p.rows.push_back(note(say(l, Text::RefRouteNone)));
        return;
    }
    const Text paidName = paid.kafra ? Text::RefRouteKafra : Text::RefRouteNpc;
    if (walk.found) routeHead(l, p, walk, Text::RefRouteWalk, code, follow);
    if (better) routeHead(l, p, paid, paidName, code, follow);
    if ((walk.found && walk.conditional) || (better && paid.conditional))
        p.rows.push_back(note(say(l, Text::RefRouteConditionNote)));
    if (walk.found) routeSteps(ix, l, p, walk, Text::RefRouteWalk, Route1, opened);
    if (better) routeSteps(ix, l, p, paid, paidName, Route2, opened);
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

int elementDamage(int attack, int defence, int level) {
    if (attack < 0 || attack > 9 || defence < 0 || defence > 9 || level < 1 || level > 4) return 100;
    return kAttrFix[level - 1][attack][defence];
}

// 3.0.11-beta.26: a card's main block - beside the next one at a share of the line (100: alone); it never folds
void asMain(Row& heading, int share) {
    heading.share = share;
    heading.link = Link{};
}

// 3.0.11-beta.26: an element's colour (the concept's chips' dots)
uint32_t elementColour(int e) {
    static const uint32_t kColours[] = {0xC4C9D4, 0x7CC0FF, 0xD8B26A, 0xFF8A5C, 0x9FE0C0,
                                        0x8BD36A, 0xFFF1A8, 0x9D8CC6, 0xC9A6FF, 0xB9A3D9};
    return e >= 0 && e <= 9 ? kColours[e] : 0xC4C9D4;
}

// 3.0.11-beta.26: an MVP - a monster with an MVP's reward (the concept's «MVP» section; a boss of mode 0x20 may not)
bool isMvp(const refindex::Mob& m) {
    return std::any_of(m.drops.begin(), m.drops.end(), [](const refindex::Drop& d) { return d.kind == 1; });
}

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

Page home(const refindex::Index& ix, Lang l, const Context& c) {
    Page p;
    p.name = say(l, Text::RefTitle);
    // 3.0.11-beta.26 (the owner:
    // «у нас оно отличается от того как это выглядит в концепте»):
    // the concept's first screen (v23_ref_home) - its search, ten sections in its order, the recent and the
    // starred cards as chips
    size_t mvps = 0, maps = 0, kafra = 0;
    for (const auto& m : ix.mobs)
        if (ix.spawnsOf.count(m.first) && isMvp(m.second)) ++mvps;
    for (const auto& m : ix.maps) {
        if (!m.second.mobs.empty()) ++maps;
        for (const refindex::Teleport& t : m.second.teleports)
            if (t.kafra) {
                ++kafra;
                break;
            }
    }
    Row search;
    search.style = Style::Search;
    search.right = "Ctrl+F";
    p.rows.push_back(search);
    Row hint;
    hint.style = Style::Hint;
    hint.left = say(l, Text::RefSearchExplain);
    p.rows.push_back(hint);
    const std::string world = c.world ? sayf(l, Text::RefWorldHintN, {std::to_string(c.world->world.size()),
                                                                      std::to_string(c.world->dungeons.size())})
                                      : say(l, Text::RefWorldHint);
    const auto count = [&](size_t n) { return number(static_cast<long long>(n), l); };
    const struct {
        const char* icon;
        Text name;
        std::string sub, count;
        Link link;
    } tiles[] = {
        {"monster", Text::RefSecMobs, say(l, Text::RefSecMobsHint), count(ix.mobs.size()), Link{Go::Section, Mobs, ""}},
        {"item", Text::RefSecItems, say(l, Text::RefSecItemsHint), count(ix.items.size()),
         Link{Go::Section, Items, ""}},
        {"location", Text::RefSecMaps, say(l, Text::RefSecMapsHint), count(maps), Link{Go::Section, Maps, ""}},
        {"quest", Text::RefSecQuests, say(l, Text::RefSecQuestsHint), ix.quests.empty() ? "" : count(ix.quests.size()),
         Link{Go::Quests, 0, ""}},
        {"npc", Text::RefSecServices, say(l, Text::RefSecServicesHint), count(ix.shops.size() + kafra),
         Link{Go::Section, Services, ""}},
        {"mvp", Text::RefSecBosses, say(l, Text::RefSecBossesHint), count(mvps), Link{Go::Section, Bosses, ""}},
        {"pet", Text::RefSecPets, say(l, Text::RefSecPetsHint), count(ix.pets.size()), Link{Go::Section, Pets, ""}},
        {"craft", Text::RefSecCraft, say(l, Text::RefSecCraftHint), count(ix.craft.size()),
         Link{Go::Section, Crafting, ""}},
        {"world", Text::RefWorld, world, "", Link{Go::World, 0, ""}},
        {"tables", Text::RefSecTables, say(l, Text::RefSecTablesHint), "", Link{Go::Section, Tables, ""}},
    };
    for (const auto& t : tiles) {
        Row r;
        r.style = Style::Tile;
        r.left = say(l, t.name);
        r.right = t.count;
        r.sub = t.sub;
        r.icon = std::string("ui:") + t.icon;
        r.link = t.link;
        p.rows.push_back(std::move(r));
    }
    const auto chips = [&](const std::vector<Link>* list) {
        std::vector<Row> out;
        if (!list) return out;
        for (const Link& k : *list) {
            Row chip;
            chip.style = Style::Chip;
            chip.left = titleOf(ix, l, k);
            if (chip.left.empty()) continue;
            chip.link = k;
            switch (k.go) {
                case Go::Mob:
                    chip.sub = say(l, Text::RefKindMob);
                    chip.icon = "mob:" + std::to_string(k.id);
                    break;
                case Go::Item:
                    chip.sub = say(l, Text::RefKindItem);
                    chip.icon = "item:" + std::to_string(k.id);
                    break;
                case Go::Map: {
                    chip.sub = say(l, Text::RefKindMap);
                    chip.icon = "ui:location";
                    const size_t code = chip.left.rfind(" (");  // «Glast Heim Churchyard», its code on its card
                    if (code != std::string::npos && code > 0) chip.left.resize(code);
                    break;
                }
                default:
                    chip.sub = say(l, Text::RefKindQuest);
                    chip.icon = "ui:quest";
                    break;
            }
            out.push_back(std::move(chip));
        }
        return out;
    };
    const std::vector<Row> recent = chips(c.recent), starred = chips(c.favourites);  // 3.0.11-beta.18
    if (!recent.empty()) {
        Row b;
        b.style = Style::Block;
        b.left = say(l, Text::RefRecent);
        b.share = 58;  // the concept's 1.4 : 1
        p.rows.push_back(b);
        p.rows.insert(p.rows.end(), recent.begin(), recent.end());
    }
    Row b;
    b.style = Style::Block;
    b.left = say(l, Text::RefFavourites);
    b.share = recent.empty() ? 0 : 42;
    p.rows.push_back(b);
    if (starred.empty()) p.rows.push_back(note(say(l, Text::RefFavEmpty)));
    p.rows.insert(p.rows.end(), starred.begin(), starred.end());
    return p;
}

namespace {

// 3.0.11-beta.28: the groups of the search's results that open (their Go::Expand bits)
enum FoundGroup : int { ByNumber, FoundMobs, FoundItems, FoundAegis, FoundMaps, FoundQuests, FoundNpcs };
constexpr size_t kFewFound = 8;  // a group's results until «ще N ›» opens it (the concept's v21_search)

// 3.0.11-beta.28: the query's words in `text` between `from` and `to` (bytes), each where the search finds it first
// (the case folded alike: the places are the text's own letters), the ones that overlap joined
Marks marksIn(const std::string& text, const std::string& query, size_t from = 0, size_t to = std::string::npos) {
    to = std::min(to, text.size());
    std::string folded;
    std::vector<std::pair<size_t, size_t>> at;  // each folded byte's letter in the text: its first byte, its end
    for (size_t i = from; i < to;) {
        size_t j = i + 1;
        while (j < to && (static_cast<unsigned char>(text[j]) & 0xC0) == 0x80) ++j;
        const std::string letter = settings::foldCase(text.substr(i, j - i));
        for (size_t k = 0; k < letter.size(); ++k) at.emplace_back(i, j);
        folded += letter;
        i = j;
    }
    std::vector<std::pair<size_t, size_t>> places;
    const std::string q = settings::foldCase(query);
    for (size_t b = 0; b < q.size();) {
        const size_t e = std::min(q.find(' ', b), q.size());
        const size_t k = e > b ? folded.find(q.substr(b, e - b)) : std::string::npos;
        if (k != std::string::npos) places.emplace_back(at[k].first, at[k + (e - b) - 1].second);
        b = e + 1;
    }
    std::sort(places.begin(), places.end());
    Marks out;
    for (const auto& [b, e] : places) {
        if (!out.empty() && b <= out.back().second)
            out.back().second = uint16_t(std::max<size_t>(out.back().second, e));
        else
            out.emplace_back(uint16_t(b), uint16_t(e));
    }
    return out;
}

// 3.0.11-beta.28: a number with its word in the form it takes («42 крамниці», «з 143 монстрів»): the text's forms
// apart by «|» - English one|more, Ukrainian and Russian one|few|many
std::string counted(Lang l, size_t n, Text t) {
    const std::string forms = say(l, t);
    const size_t form = l == Lang::En                                                 ? (n == 1 ? 0 : 1)
                        : n % 10 == 1 && n % 100 != 11                                ? 0
                        : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 12 || n % 100 > 14) ? 1
                                                                                       : 2;
    size_t b = 0;
    for (size_t k = 0; k < form; ++k) {
        const size_t bar = forms.find('|', b);
        if (bar == std::string::npos) break;
        b = bar + 1;
    }
    return number(static_cast<long long>(n), l) + " " + forms.substr(b, forms.find('|', b) - b);
}

// 3.0.11-beta.28: one found as the concept's row: its icon, its name with the query marked, its line, its kind
Row result(const std::string& name, const std::string& sub, Text kind, Link to, std::string icon,
           const std::string& query, Lang l) {
    Row r;
    r.style = Style::Result;
    r.left = name;
    r.sub = sub;
    r.right = say(l, kind);
    r.icon = std::move(icon);
    r.link = std::move(to);
    r.marks = marksIn(name, query);
    return r;
}

// A result found by another name than its own: the query marked there, in its line
void markedIn(Row& r, const std::string& name, const std::string& query) {
    r.marks.clear();
    const size_t at = r.sub.find(name);
    if (at != std::string::npos) r.subMarks = marksIn(r.sub, query, at, at + name.size());
}

std::string joined(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& part : parts)
        if (!part.empty()) out += (out.empty() ? "" : " · ") + part;
    return out;
}

template <typename List>
size_t distinct(const List& list, int (*of)(const typename List::value_type&)) {
    std::set<int> seen;
    for (const auto& v : list) seen.insert(of(v));
    return seen.size();
}

// A monster: the other name it was found by, MVP or a boss, its level, on how many maps
Row mobFound(const refindex::Index& ix, Lang l, const refindex::Hit& h, const std::string& query) {
    const refindex::Mob& m = ix.mobs.at(h.id);
    std::vector<std::string> parts;
    if (h.by == 1) parts.push_back(sayf(l, Text::RefOtherName, {m.other}));
    if (isMvp(m)) parts.push_back("MVP");
    else if (m.boss) parts.push_back(say(l, Text::RefBoss));
    parts.push_back(sayf(l, Text::RefLevel, {std::to_string(m.lv)}));
    const auto where = ix.spawnsOf.find(h.id);
    if (where != ix.spawnsOf.end() && !where->second.empty())
        parts.push_back(counted(l, where->second.size(), Text::RefMapsN));
    Row r = result(m.name, joined(parts), Text::RefKindMob, toMob(h.id), "mob:" + std::to_string(h.id), query, l);
    r.gold = m.boss;
    if (h.by == 1) markedIn(r, m.other, query);
    return r;
}

// An item: its service name when found by it; else the cheapest shop's price and the shops (or its kind), and from
// how many monsters it drops
Row itemFound(const refindex::Index& ix, Lang l, const refindex::Hit& h, const std::string& query) {
    const refindex::Item& it = ix.items.at(h.id);
    std::vector<std::string> parts;
    if (h.by == 2) {
        parts.push_back(sayf(l, Text::RefServiceName, {it.aegis}));
    } else {
        const auto at = ix.soldAt.find(h.id);
        if (at != ix.soldAt.end() && !at->second.empty()) {
            parts.push_back(number(at->second.front().second, l) + " z");
            parts.push_back(counted(l, distinct(at->second, +[](const std::pair<int, int>& s) { return s.first; }),
                                    Text::RefShopsN));
        } else if (it.type != 3) {  // not «Інше»
            parts.push_back(typeName(it.type, l));
        }
        const auto by = ix.droppedBy.find(h.id);
        if (by != ix.droppedBy.end() && !by->second.empty()) {
            const size_t mobs = distinct(by->second, +[](const refindex::Source& s) { return s.mob; });
            parts.push_back(sayf(l, Text::RefDropsFrom, {counted(l, mobs, Text::RefFromMobsN)}));
        }
    }
    Row r = result(it.name, joined(parts), Text::RefKindItem, toItem(h.id), "item:" + std::to_string(h.id), query, l);
    if (h.by == 2) markedIn(r, it.aegis, query);
    return r;
}

// A location: the player's, a town, its monsters' kinds, the players there now
Row mapFound(const refindex::Index& ix, Lang l, const std::string& code, const Context& c, const std::string& query) {
    std::vector<std::string> parts;
    if (code == c.here) parts.push_back(say(l, Text::RefYouHere));
    const auto it = ix.maps.find(code);
    if (it != ix.maps.end() &&
        std::find(it->second.flags.begin(), it->second.flags.end(), "town") != it->second.flags.end())
        parts.push_back(say(l, Text::RefFlagTown));
    parts.push_back(mapNote(ix, l, code));
    if (c.players) {
        const auto now = c.players->find(code);
        if (now != c.players->end() && now->second > 0)
            parts.push_back(sayf(l, Text::RefPlayersNow, {std::to_string(now->second)}));
    }
    Row r = result(refindex::mapTitle(ix, code), joined(parts), Text::RefKindMap, toMap(code), "ui:location", query, l);
    r.gold = code == c.here;
    return r;
}

}  // namespace

Page found(const refindex::Index& ix, Lang l, const std::string& query, const Context& c, unsigned opened) {
    Page p;
    p.name = "«" + query + "»";
    Row field;  // 3.0.11-beta.26: the concept's results under the search's own field (v21_search), Esc its key
    field.style = Style::Search;
    field.right = "Esc";
    p.rows.push_back(field);
    // 3.0.11-beta.28: a group - its capitals and its note, its first kFewFound results (only the rows shown made)
    // and «ще N ›» that opens it; `count` the results it may show (kMostFound at most) of `all` found
    auto group = [&](FoundGroup g, const std::string& title, const std::string& aside, size_t count, size_t all,
                     const std::function<Row(size_t)>& make) {
        if (count == 0) return;
        Row head;
        head.style = Style::Group;
        head.left = title;
        head.right = aside;
        p.rows.push_back(std::move(head));
        const bool open = ((opened >> unsigned(g)) & 1u) || count <= kFewFound + 1;
        const size_t n = open ? count : kFewFound;
        for (size_t i = 0; i < n; ++i) p.rows.push_back(make(i));
        if (n < count) {
            Row more;
            more.style = Style::More;
            more.left = sayf(l, Text::RefFoundMore, {std::to_string(count - n)});
            more.link = Link{Go::Expand, int(g), ""};
            p.rows.push_back(std::move(more));
        } else if (all > count) {
            p.rows.push_back(note(sayf(l, Text::RefShownOf, {std::to_string(count), std::to_string(all)})));
        }
    };
    const bool digits = !query.empty() && query.size() <= 6 &&
                        std::all_of(query.begin(), query.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
    if (digits) {  // 3.0.11-beta.22: a number - the item and the monster of that id (as @iteminfo, @mobinfo say them)
        const int id = std::atoi(query.c_str());
        std::vector<Row> rows;
        if (ix.items.count(id)) rows.push_back(itemFound(ix, l, refindex::Hit{refindex::Kind::Item, id, ""}, query));
        if (ix.mobs.count(id)) rows.push_back(mobFound(ix, l, refindex::Hit{refindex::Kind::Mob, id, ""}, query));
        group(ByNumber, sayf(l, Text::RefById, {query}), "", rows.size(), rows.size(),
              [&](size_t i) { return rows[i]; });
    }
    const refindex::Found f = refindex::search(ix, query, kMostFound);
    group(FoundMobs, sayf(l, Text::RefFoundMobs, {std::to_string(f.mobsAll)}), "", f.mobs.size(), f.mobsAll,
          [&](size_t i) { return mobFound(ix, l, f.mobs[i], query); });
    group(FoundItems, sayf(l, Text::RefFoundItems, {std::to_string(f.itemsAll)}), say(l, Text::RefOftenFirst),
          f.items.size(), f.itemsAll, [&](size_t i) { return itemFound(ix, l, f.items[i], query); });
    group(FoundAegis, sayf(l, Text::RefFoundAegis, {std::to_string(f.aegisAll)}), "", f.aegis.size(), f.aegisAll,
          [&](size_t i) { return itemFound(ix, l, f.aegis[i], query); });  // 3.0.11-beta.28
    group(FoundMaps, sayf(l, Text::RefFoundMaps, {std::to_string(f.mapsAll)}), "", f.maps.size(), f.mapsAll,
          [&](size_t i) { return mapFound(ix, l, f.maps[i].map, c, query); });
    std::vector<std::pair<size_t, bool>> quests;  // 3.0.11-beta.16: by their titles (the interface's, English's)
    size_t questsAll = 0;
    const std::string folded = settings::foldCase(query);
    for (size_t i = 0; i < ix.quests.size(); ++i) {
        const refquests::Quest& q = ix.quests[i];
        const bool own = settings::textMatches(settings::foldCase(refquests::textOf(q.title, questLang(l))), folded);
        if (!own && !settings::textMatches(settings::foldCase(q.title[refquests::En]), folded)) continue;
        if (++questsAll <= kMostFound) quests.emplace_back(i, own);
    }
    group(FoundQuests, sayf(l, Text::RefFoundQuests, {std::to_string(questsAll)}), "", quests.size(), questsAll,
          [&](size_t k) {
              const auto [i, own] = quests[k];
              const refquests::Quest& q = ix.quests[i];
              std::vector<std::string> parts;
              if (!own) parts.push_back(q.title[refquests::En]);  // found by its English title: that title, marked
              parts.push_back(categoryName(q.category, l));
              if (q.minLevel > 0) parts.push_back(sayf(l, Text::RefQuestLevel, {std::to_string(q.minLevel)}));
              parts.push_back(q.npc);
              const std::string& title = refquests::textOf(q.title, questLang(l));
              Row r = result(title, joined(parts), Text::RefKindQuest, toQuest(int(i)), "ui:quest", query, l);
              if (!own) markedIn(r, q.title[refquests::En], query);
              return r;
          });
    if (!f.npcs.empty()) {  // 3.0.11-beta.15: the nearest to the player's map first («kafra» - the Kafra to walk to)
        const auto moves =
            c.here.empty() ? std::unordered_map<std::string, refroute::Far>{}
                           : refroute::movesFrom(ix, c.here, c.x, c.y);
        auto far = [&](const std::string& map) {
            if (map == c.here) return 0;
            const auto m = moves.find(map);
            return m == moves.end() ? INT_MAX : m->second.moves * 2 + (m->second.paid ? 1 : 0);
        };
        std::vector<refindex::Hit> npcs = f.npcs;
        std::stable_sort(npcs.begin(), npcs.end(), [&](const refindex::Hit& a, const refindex::Hit& b) {
            return far(a.map) < far(b.map);
        });
        group(FoundNpcs, sayf(l, Text::RefFoundNpcs, {std::to_string(f.npcsAll)}),
              c.here.empty() ? "" : say(l, Text::RefNearFirst), std::min(npcs.size(), kMostFound), f.npcsAll,
              [&](size_t i) {
                  const refindex::Hit& h = npcs[i];
                  const refindex::Npc& n = ix.maps.at(h.map).npcs[size_t(h.id)];
                  std::string sub =
                      refindex::mapTitle(ix, h.map) + " · " + std::to_string(n.x) + ", " + std::to_string(n.y);
                  const auto m = moves.find(h.map);
                  if (h.map == c.here) sub = say(l, Text::RefYouHere) + " · " + sub;
                  else if (m != moves.end()) sub += " · " + farNote(m->second, l);
                  Row r = result(n.name, sub, Text::RefKindNpc, toMap(h.map), "ui:npc", query, l);
                  r.dot = refindex::colorOf(n.kind);
                  return r;
              });
    }
    Row end;  // 3.0.11-beta.28: how the search finds (a line above it), or nothing found and what to try
    end.style = p.rows.size() == 1 ? Style::Empty : Style::Hint;
    end.left = p.rows.size() == 1 ? sayf(l, Text::RefNothingFor, {query}) : say(l, Text::RefSearchExplain);
    if (end.style == Style::Empty) end.sub = say(l, Text::RefNothingHint);
    p.rows.push_back(std::move(end));
    return p;
}

namespace {

// The shops by their maps' names; Kafra's teleports by the towns (3.0.11-beta.26: «NPC і послуги» has both)
std::vector<Row> shopRows(const refindex::Index& ix, Lang l) {
    std::vector<Row> out;
    std::vector<std::tuple<std::string, std::string, int>> list;  // the map's title, the shop, its index
    for (size_t s = 0; s < ix.shops.size(); ++s)
        list.emplace_back(refindex::mapTitle(ix, ix.shops[s].map), ix.shops[s].name, int(s));
    std::sort(list.begin(), list.end());
    for (const auto& [title, name, s] : list) {
        const std::string goods = std::to_string(ix.shops[size_t(s)].items.size());
        out.push_back(linked(name + " · " + title, sayf(l, Text::RefItemsCount, {goods}), Link{Go::Shop, s, ""}));
    }
    return out;
}

std::vector<Row> kafraRows(const refindex::Index& ix) {
    std::vector<Row> out;
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
        out.push_back(linked(refindex::mapTitle(ix, m.first), right, toMap(m.first)));
    }
    std::sort(out.begin(), out.end(), [](const Row& a, const Row& b) { return a.left < b.left; });
    return out;
}

}  // namespace

Page section(const refindex::Index& ix, Lang l, int id, const Context& c) {
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
                if (!isMvp(m.second) || where == ix.spawnsOf.end()) continue;  // 3.0.11-beta.26: «MVP»
                int longest = 0;
                for (const refindex::Where& w : where->second) longest = std::max(longest, w.respawnMs);
                list.emplace_back(-longest, m.second.name, m.first);
            }
            std::sort(list.begin(), list.end());
            for (const auto& [neg, name, mob] : list) {
                const refindex::Where& w = ix.spawnsOf.at(mob).front();
                std::string right = refindex::mapTitle(ix, w.map);
                if (-neg >= 600000) right += " · " + minutes(l, -neg);
                // 3.0.11-beta.20: the board of MVP hunters - a player's public timer of it (the latest)
                const reflive::Timer* t = c.live ? reflive::timerOf(*c.live, mob) : nullptr;
                const long long respawn = t ? respawnOn(ix, mob, t->map) / 1000 : 0;
                if (t && respawn > 0 && c.now > 0) {
                    const long long left = t->killed + respawn - c.now;
                    right += " · " + (left > 0 ? sayf(l, Text::RefTimerSoon, {respawnText(left, l)})
                                               : say(l, Text::RefTimerDue));
                }
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
        case Shops:
            p.name = say(l, Text::RefSecShops);
            p.rows.push_back(heading(p.name));
            for (Row& r : shopRows(ix, l)) p.rows.push_back(std::move(r));
            break;
        case Teleports:
            p.name = say(l, Text::RefSecTeleports);
            p.rows.push_back(heading(p.name));
            for (Row& r : kafraRows(ix)) p.rows.push_back(std::move(r));
            break;
        case Services:  // 3.0.11-beta.26: the concept's «NPC і послуги» - the shops and Kafra
            p.name = say(l, Text::RefSecServices);
            p.rows.push_back(heading(say(l, Text::RefSecTeleports)));
            for (Row& r : kafraRows(ix)) p.rows.push_back(std::move(r));
            p.rows.push_back(heading(say(l, Text::RefSecShops)));
            for (Row& r : shopRows(ix, l)) p.rows.push_back(std::move(r));
            break;
        case Tables:  // 3.0.11-beta.26: the concept's «Таблиці» - the server's element table, each level
            p.name = say(l, Text::RefSecTables);
            for (int lv = 1; lv <= 4; ++lv) {
                p.rows.push_back(heading(sayf(l, Text::RefTableElements, {std::to_string(lv)})));
                for (int attack = 0; attack < 10; ++attack) {
                    std::string right;
                    for (int defence = 0; defence < 10; ++defence) {
                        const int pct = elementDamage(attack, defence, lv);
                        if (pct == 100) continue;
                        right += (right.empty() ? "" : " · ") + elementName(defence, l) + " " +
                                 (pct < 0 ? sayf(l, Text::RefElementHeals, {std::to_string(-pct)})
                                          : std::to_string(pct) + " %");
                    }
                    p.rows.push_back(linked(elementName(attack, l), right.empty() ? say(l, Text::RefTableRest) : right,
                                            Link{}));
                }
            }
            break;
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

Page mob(const refindex::Index& ix, Lang l, int id, const Context& c, unsigned opened) {
    const std::string& here = c.here;
    Page p;
    const auto it = ix.mobs.find(id);
    if (it == ix.mobs.end()) return p;
    const refindex::Mob& m = it->second;
    p.name = m.name;
    Row title;
    title.style = Style::Title;
    title.left = m.name;
    title.gold = m.boss;
    p.rows.push_back(title);
    Row hero;  // 3.0.11-beta.14: beside its picture (the client's sprite); 3.0.11-beta.26 the concept's tags, its line
    hero.style = Style::Portrait;
    hero.picture = "mob:" + std::to_string(id);
    hero.tags = {sayf(l, Text::RefLevel, {std::to_string(m.lv)}), "el" + std::to_string(m.element) + ":" +
                                                                     elementName(m.element, l) + " " +
                                                                     std::to_string(m.elementLv)};
    if (isMvp(m)) hero.tags.push_back("gold:MVP");
    else if (m.boss) hero.tags.push_back("gold:" + say(l, Text::RefBoss));
    hero.left = raceName(m.race, l) + " · " + sizeName(m.size, l) + " · " + sayf(l, Text::RefHp, {number(m.hp, l)}) +
                " · " + sayf(l, Text::RefExp, {number(m.baseExp, l), number(m.jobExp, l)});
    p.rows.push_back(hero);

    // the concept's two main blocks side by side: where it lives, what it drops; the rest folds
    const auto where = ix.spawnsOf.find(id);
    p.rows.push_back(heading(say(l, Text::RefWhere), "", Where));
    asMain(p.rows.back(), 50);
    if (where == ix.spawnsOf.end() || where->second.empty()) {
        p.rows.push_back(note(say(l, Text::RefNowhere)));
    } else {
        std::vector<refindex::Where> list = where->second;
        std::stable_partition(list.begin(), list.end(), [&](const refindex::Where& w) { return w.map == here; });
        const auto moves =
            here.empty() ? std::unordered_map<std::string, refroute::Far>{} : refroute::movesFrom(ix, here);
        std::vector<Row> rows;
        for (const refindex::Where& w : list) {
            std::string right = "×" + std::to_string(w.count);
            if (w.respawnMs >= 600000) right += " · " + minutes(l, w.respawnMs);
            if (w.map == here) right = say(l, Text::RefYouHere) + " · " + right;
            const auto far = moves.find(w.map);  // 3.0.11-beta.12: how far from here
            if (w.map != here && far != moves.end()) right += " · " + farNote(far->second, l);
            Row r = linked(refindex::mapTitle(ix, w.map), right, toMap(w.map));
            r.gold = w.map == here;
            rows.push_back(std::move(r));
        }
        for (Row& r : p.rows)  // the heading's count: its maps
            if (r.style == Style::Heading && r.fold == Where) r.right = std::to_string(list.size());
        capped(p, std::move(rows), Where, opened, l);
        p.rows.push_back(linked(say(l, Text::RefOnWorld), "", Link{Go::World, 0, "mob:" + std::to_string(id)}));
    }
    p.rows.push_back(heading(say(l, Text::RefDrops), m.drops.empty() ? "" : std::to_string(m.drops.size()), Drops));
    asMain(p.rows.back(), 50);
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
    elements(l, p, m);          // 3.0.11-beta.13
    timers(ix, l, p, id, c);    // 3.0.11-beta.12
    for (const refindex::Pet& pet : ix.pets) {  // a pet: what tames it, its egg, its food, its accessory
        if (pet.mob != id) continue;
        p.rows.push_back(heading(say(l, Text::RefPet), "", PetOf));
        const std::pair<int, Text> parts[] = {{pet.lure, Text::RefPetLure}, {pet.egg, Text::RefPetEgg},
                                              {pet.food, Text::RefPetFood}, {pet.equip, Text::RefPetEquip}};
        for (const auto& [item, what] : parts)
            if (item > 0) p.rows.push_back(linked(itemName(ix, item), say(l, what), toItem(item)));
    }
    return p;
}

Page item(const refindex::Index& ix, Lang l, int id, const Context& c, unsigned opened) {
    const std::string& here = c.here;
    Page p;
    const auto it = ix.items.find(id);
    if (it == ix.items.end()) return p;
    const refindex::Item& i = it->second;
    p.name = i.name;
    Row title;
    title.style = Style::Title;
    title.left = i.name + (i.slots > 0 ? " [" + std::to_string(i.slots) + "]" : "");
    p.rows.push_back(title);
    Row hero;  // 3.0.11-beta.14: beside its icon (the client's); 3.0.11-beta.26 the concept's tags, its line
    hero.style = Style::Portrait;
    hero.picture = "item:" + std::to_string(id);
    hero.tags = {typeName(i.type, l)};
    if (!i.aegis.empty()) hero.tags.push_back(i.aegis);
    hero.left = sayf(l, Text::RefWeight, {number(i.weight / 10, l) + (i.weight % 10 ? (l == Lang::En ? "." : ",") +
                                                                                        std::to_string(i.weight % 10)
                                                                                  : "")});
    if (i.buy > 0) hero.left += " · " + sayf(l, Text::RefPrice, {number(i.buy, l)});
    if (i.buy > 1) hero.left += " · " + sayf(l, Text::RefSells, {number(i.buy / 2, l)});  // 3.0.11-beta.22: an NPC's
    if (i.slots > 0) hero.left += " · " + sayf(l, Text::RefSlots, {std::to_string(i.slots)});
    p.rows.push_back(hero);

    // the concept's main blocks: who drops it, where to buy it (side by side when both); the rest folds
    const auto by = ix.droppedBy.find(id);
    const auto at = ix.soldAt.find(id);
    const int share = by != ix.droppedBy.end() && at != ix.soldAt.end() ? 50 : 100;
    if (by != ix.droppedBy.end()) {
        p.rows.push_back(heading(say(l, Text::RefDroppedBy), std::to_string(by->second.size()), DroppedBy));
        asMain(p.rows.back(), share);
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
        p.rows.push_back(linked(say(l, Text::RefOnWorld), "", Link{Go::World, 0, "item:" + std::to_string(id)}));
    }
    if (at != ix.soldAt.end()) {
        p.rows.push_back(heading(say(l, Text::RefSoldAt), std::to_string(at->second.size()), SoldAt));
        asMain(p.rows.back(), share);
        // 3.0.11-beta.12: of the cheapest the nearest first (your map's, then by the moves from it)
        const auto moves =
            here.empty() ? std::unordered_map<std::string, refroute::Far>{} : refroute::movesFrom(ix, here);
        auto far = [&](const std::string& map) {  // on foot before paying for as many moves
            if (map == here) return 0;
            const auto m = moves.find(map);
            return m == moves.end() ? INT_MAX : m->second.moves * 2 + (m->second.paid ? 1 : 0);
        };
        std::vector<std::pair<int, int>> sold = at->second;
        std::stable_sort(sold.begin(), sold.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) {
            if (a.second != b.second) return a.second < b.second;
            return far(ix.shops[size_t(a.first)].map) < far(ix.shops[size_t(b.first)].map);
        });
        std::vector<Row> rows;
        for (const auto& [s, price] : sold) {
            const refindex::Shop& shop = ix.shops[size_t(s)];
            std::string right = sayf(l, Text::RefZeny, {number(price, l)});
            const auto m = moves.find(shop.map);
            if (shop.map == here) right = say(l, Text::RefYouHere) + " · " + right;
            else if (m != moves.end()) right += " · " + farNote(m->second, l);
            Row r = linked(shop.name + " · " + refindex::mapTitle(ix, shop.map), right, Link{Go::Shop, s, ""});
            r.gold = shop.map == here;
            rows.push_back(std::move(r));
        }
        capped(p, std::move(rows), SoldAt, opened, l);
    }
    market(l, p, id, c);                       // 3.0.11-beta.12
    const auto needed = ix.questsOf.find(id);  // 3.0.11-beta.16: the quests that take it
    if (needed != ix.questsOf.end()) {
        p.rows.push_back(heading(say(l, Text::RefQuestFor), std::to_string(needed->second.size()), QuestFor));
        for (int q : needed->second) {
            int n = 0;
            for (const auto& [item, k] : ix.quests[size_t(q)].items)
                if (item == id) n = k;
            p.rows.push_back(linked(refquests::textOf(ix.quests[size_t(q)].title, questLang(l)), "×" + number(n, l),
                                    toQuest(q)));
        }
    }
    const auto made = ix.craftOf.find(id);
    if (made != ix.craftOf.end()) {
        p.rows.push_back(heading(say(l, Text::RefCraftOf), "", CraftOf));
        for (int c : made->second)
            for (const auto& [mat, n] : ix.craft[size_t(c)].mats)
                p.rows.push_back(linked(itemName(ix, mat), "×" + std::to_string(n), toItem(mat)));
    }
    const auto used = ix.usedIn.find(id);
    if (used != ix.usedIn.end()) {
        p.rows.push_back(heading(say(l, Text::RefUsedIn), std::to_string(used->second.size()), UsedIn));
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
        p.rows.push_back(heading(say(l, Text::RefPet), "", PetOf));
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
    title.gold = code == here;
    p.rows.push_back(title);
    const auto it = ix.maps.find(code);
    Row hero;  // 3.0.11-beta.26: the concept's - its minimap's picture (the client's), its tags, its line
    hero.style = Style::Portrait;
    hero.picture = "map:" + code;
    hero.tags = {code};
    if (named != ix.mapNames.end() && refindex::shortName(named->second) != named->second) hero.left = named->second;
    if (it != ix.maps.end()) {
        const std::pair<const char*, Text> flags[] = {
            {"town", Text::RefFlagTown},         {"pvp", Text::RefFlagPvp},        {"gvg", Text::RefFlagGvg},
            {"noteleport", Text::RefFlagNoTeleport}, {"nomemo", Text::RefFlagNoMemo}, {"nosave", Text::RefFlagNoSave}};
        for (const auto& [flag, words] : flags)
            if (std::find(it->second.flags.begin(), it->second.flags.end(), flag) != it->second.flags.end())
                hero.tags.push_back(say(l, words));
    }
    if (code == here) hero.tags.push_back("gold:" + say(l, Text::RefYouHere));
    if (c.players) {  // 3.0.11-beta.10: the site's «Онлайн» now
        const auto now = c.players->find(code);
        if (now != c.players->end() && now->second > 0)
            hero.left +=
                (hero.left.empty() ? "" : " · ") + sayf(l, Text::RefPlayersNow, {std::to_string(now->second)});
    }
    p.rows.push_back(hero);
    // the concept's main blocks: who is here, how to get here (side by side when both); the rest folds
    const bool way = !here.empty() && here != code && ix.graph;
    const bool who = it != ix.maps.end() && !it->second.mobs.empty();
    if (!who && way) {
        const size_t at = p.rows.size();
        route(ix, l, p, here, c.x, c.y, code, opened, c.follow);
        if (p.rows.size() > at) asMain(p.rows[at], 100);
    }
    if (it == ix.maps.end()) {
        kafraNear(ix, l, p, code);  // 3.0.11-beta.15
        return p;
    }
    const refindex::MapHints& h = it->second;
    if (who) {
        p.rows.push_back(heading(say(l, Text::RefWhoHere), std::to_string(h.mobs.size()), WhoHere));
        asMain(p.rows.back(), way ? 50 : 100);
        std::vector<Row> rows;
        for (const refindex::MobOnMap& o : h.mobs) {
            const auto m = ix.mobs.find(o.id);
            std::string right = "×" + std::to_string(o.count);
            if (m != ix.mobs.end()) right += " · " + sayf(l, Text::RefLevel, {std::to_string(m->second.lv)});
            if (m != ix.mobs.end() && m->second.boss && o.respawnMs >= 600000)
                right += " · " + minutes(l, o.respawnMs);
            Row r = linked(mobName(ix, o.id), right, toMob(o.id));
            r.gold = m != ix.mobs.end() && m->second.boss;
            r.icon = "mob:" + std::to_string(o.id);  // 3.0.11-beta.26: the concept's - its picture before it
            rows.push_back(std::move(r));
        }
        capped(p, std::move(rows), WhoHere, opened, l);
    }
    if (who && way) {
        const size_t at = p.rows.size();
        route(ix, l, p, here, c.x, c.y, code, opened, c.follow);
        if (p.rows.size() > at) asMain(p.rows[at], 50);
    }
    kafraNear(ix, l, p, code);  // 3.0.11-beta.15
    std::vector<std::string> exits;
    for (const refindex::Warp& w : h.warps)
        if (std::find(exits.begin(), exits.end(), w.dest) == exits.end()) exits.push_back(w.dest);
    if (!exits.empty()) {
        p.rows.push_back(heading(say(l, Text::RefExits), std::to_string(exits.size()), Exits));
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
        p.rows.push_back(heading(say(l, Text::RefTeleports), std::to_string(teleports.size()), Teleports2));
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
        p.rows.push_back(heading(say(l, Text::RefShopsHere), std::to_string(shops.size()), ShopsHere));
        std::vector<Row> rows;
        for (int s : shops)
            rows.push_back(linked(ix.shops[size_t(s)].name,
                                  sayf(l, Text::RefItemsCount, {std::to_string(ix.shops[size_t(s)].items.size())}),
                                  Link{Go::Shop, s, ""}));
        capped(p, std::move(rows), ShopsHere, opened, l);
    }
    if (!h.npcs.empty()) {
        p.rows.push_back(heading(say(l, Text::RefNpcs), std::to_string(h.npcs.size()), Npcs));
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

Page quests(const refindex::Index& ix, Lang l) {
    Page p;
    p.name = say(l, Text::RefSecQuests);
    const int ql = questLang(l);
    std::string kind;
    for (size_t i = 0; i < ix.quests.size(); ++i) {  // the site's order: by kind, then level
        const refquests::Quest& q = ix.quests[i];
        const std::string name = categoryName(q.category, l);
        if (name != kind) {
            kind = name;
            p.rows.push_back(heading(name));
        }
        std::string right = q.minLevel > 0 ? sayf(l, Text::RefQuestLevel, {std::to_string(q.minLevel)}) : "";
        if (!q.npcMap.empty()) right += (right.empty() ? "" : " · ") + q.npcMap;
        p.rows.push_back(linked(refquests::textOf(q.title, ql), right, toQuest(int(i))));
    }
    if (p.rows.empty()) p.rows.push_back(note(say(l, Text::RefQuestsNone)));
    return p;
}

Page quest(const refindex::Index& ix, Lang l, int index) {
    Page p;
    if (index < 0 || size_t(index) >= ix.quests.size()) return p;
    const refquests::Quest& q = ix.quests[size_t(index)];
    const int ql = questLang(l);
    p.name = refquests::textOf(q.title, ql);
    Row title;
    title.style = Style::Title;
    title.left = p.name;
    p.rows.push_back(title);
    Row hero;  // 3.0.11-beta.26: the concept's - its tags (the kind, the level), who gives it and where
    hero.style = Style::Portrait;
    hero.tags = {categoryName(q.category, l)};
    if (q.minLevel > 0) hero.tags.push_back(sayf(l, Text::RefQuestLevel, {std::to_string(q.minLevel)}));
    hero.left = q.npc + (q.npcMap.empty() ? "" : (q.npc.empty() ? "" : " · ") + refindex::mapTitle(ix, q.npcMap));
    p.rows.push_back(hero);
    if (!refquests::textOf(q.summary, ql).empty()) p.rows.push_back(text(refquests::textOf(q.summary, ql)));
    // the concept's: each part a block the whole width (none folds)
    if (!q.npc.empty()) {
        p.rows.push_back(heading(say(l, Text::RefQuestNpc), "", QuestNpc));
        asMain(p.rows.back(), 100);
        Row r = q.npcMap.empty() ? linked(q.npc, "", Link{}) : linked(q.npc, refindex::mapTitle(ix, q.npcMap),
                                                                         toMap(q.npcMap));
        r.dot = refindex::colorOf(refindex::NpcKind::Guide);
        p.rows.push_back(std::move(r));
    }
    if (!q.items.empty()) {
        p.rows.push_back(heading(say(l, Text::RefQuestItems), std::to_string(q.items.size()), QuestItems));
        asMain(p.rows.back(), 100);
        for (const auto& [item, n] : q.items)
            p.rows.push_back(linked(itemName(ix, item), "×" + number(n, l), toItem(item)));
    }
    const std::vector<std::string>& steps = refquests::listOf(q.steps, ql);
    if (!steps.empty()) {
        p.rows.push_back(heading(say(l, Text::RefQuestSteps), std::to_string(steps.size()), QuestSteps));
        asMain(p.rows.back(), 100);
        for (size_t i = 0; i < steps.size(); ++i) p.rows.push_back(text(std::to_string(i + 1) + ". " + steps[i]));
    }
    const std::vector<std::string>& rewards = refquests::listOf(q.rewards, ql);
    if (!rewards.empty()) {
        p.rows.push_back(heading(say(l, Text::RefQuestRewards), "", QuestReward));
        asMain(p.rows.back(), 100);
        for (const std::string& r : rewards) p.rows.push_back(text(r));
    }
    return p;
}

std::map<std::string, int> worldMarks(const refindex::Index& ix, const std::string& marks) {
    std::map<std::string, int> out;
    auto addMob = [&](int mob) {
        const auto where = ix.spawnsOf.find(mob);
        if (where != ix.spawnsOf.end())
            for (const refindex::Where& w : where->second) out[w.map] += w.count;
    };
    if (marks.rfind("mob:", 0) == 0) addMob(std::atoi(marks.c_str() + 4));
    if (marks.rfind("item:", 0) == 0) {  // 3.0.11-beta.20: where to farm it - its droppers' maps, how many of them
        const auto by = ix.droppedBy.find(std::atoi(marks.c_str() + 5));
        if (by != ix.droppedBy.end())
            for (const refindex::Source& s : by->second) addMob(s.mob);
    }
    return out;
}

Page world(const refindex::Index& ix, Lang l, const Context& c, const std::string& marks) {
    Page p;
    p.name = say(l, Text::RefWorld);
    Row title;
    title.style = Style::Title;
    title.left = say(l, Text::RefWorld);
    const int mob = marks.rfind("mob:", 0) == 0 ? std::atoi(marks.c_str() + 4) : 0;
    const int item = marks.rfind("item:", 0) == 0 ? std::atoi(marks.c_str() + 5) : 0;
    if (mob > 0) title.left = sayf(l, Text::RefWorldOf, {mobName(ix, mob)});  // 3.0.11-beta.19
    if (item > 0) title.left = sayf(l, Text::RefWorldOf, {itemName(ix, item)});  // 3.0.11-beta.20
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
    const std::map<std::string, int> marked = worldMarks(ix, marks);
    const worldmap::Tile* t = c.hover.empty() ? nullptr : worldmap::tileOf(*c.world, c.hover);
    if (!t) {
        p.rows.push_back(note(say(l, Text::RefWorldHelp)));
    } else {
        std::string line = refindex::mapTitle(ix, t->map);
        const auto m = marked.find(t->map);
        if (m != marked.end())
            line += " · " + (mob > 0 ? mobName(ix, mob) + " ×" + std::to_string(m->second)
                                     : sayf(l, Text::RefDroppersHere, {std::to_string(m->second)}));
        const std::string more = mapLine(ix, l, t->map, c);
        if (!more.empty()) line += " · " + more;
        if (t->map == c.here) line += " · " + say(l, Text::RefYouHere);
        p.rows.push_back(note(line));
    }
    std::vector<Row> under;  // the marked maps not on the world's sheet: dungeons, rooms
    for (const auto& [map, n] : marked)
        if (!worldmap::tileOf(*c.world, map))
            under.push_back(linked(refindex::mapTitle(ix, map), "×" + std::to_string(n), toMap(map)));
    if (!under.empty()) {
        p.rows.push_back(heading(say(l, Text::RefAlsoInside), std::to_string(under.size())));
        for (Row& r : under) p.rows.push_back(std::move(r));
    }
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

namespace {

// 3.0.11-beta.17: the sections the player folded - their headings alone («29 ›»), their rows left out
Page folded(Page p, unsigned bits) {
    if (!bits) return p;
    std::vector<Row> rows;
    bool skip = false;
    for (Row& r : p.rows) {
        const bool head = r.style == Style::Heading || r.style == Style::Title || r.style == Style::Portrait;
        if (head) skip = false;
        if (skip) continue;
        // 3.0.11-beta.26: drawn closed (▸); a card's main block (a share) never folds
        if (r.style == Style::Heading && r.fold >= 0 && r.share == 0 && ((bits >> unsigned(r.fold)) & 1u)) {
            r.closed = true;
            skip = true;
        }
        rows.push_back(std::move(r));
    }
    p.rows = std::move(rows);
    return p;
}

Page card(const refindex::Index& ix, Lang l, const Link& link, const Context& c, unsigned opened);

}  // namespace

Page open(const refindex::Index& ix, Lang l, const Link& link, const Context& c, unsigned opened) {
    Page p = folded(card(ix, l, link, c, opened), c.folded);
    const bool card = link.go == Go::Mob || link.go == Go::Item || link.go == Go::Map || link.go == Go::Quest;
    if (card && c.favourites && !p.rows.empty() && p.rows[0].style == Style::Title)  // 3.0.11-beta.18
        p.star = std::any_of(c.favourites->begin(), c.favourites->end(),  // 3.0.11-beta.26: the header's star
                             [&](const Link& f) { return sameCard(f, link); })
                     ? 1
                     : 0;
    return p;
}

Page withRail(Page p, const refindex::Index& ix, Lang l, const Link& at) {
    Row all;  // the search's, the first screen's
    all.style = Style::Tile;
    all.left = say(l, Text::RefRailAll);
    all.icon = "ui:search";
    p.rail = {all};
    for (Row& r : home(ix, l).rows)
        if (r.style == Style::Tile && r.link.go != Go::World) p.rail.push_back(std::move(r));
    // the page's own section: a card's kind, a section's (its parts: the bosses MVP, the shops and Kafra the NPCs',
    // the cards and the types the items')
    int section = -1;
    Go go = at.go;
    switch (at.go) {
        case Go::Mob: section = Mobs; break;
        case Go::Item: section = Items; break;
        case Go::Map: section = Maps; break;
        case Go::Shop: section = Services; break;
        case Go::Quest: go = Go::Quests; break;
        case Go::Section:
            section = at.id >= 100 || at.id == Cards ? Items
                      : at.id == Shops || at.id == Teleports ? Services
                                                             : at.id;
            break;
        default: break;
    }
    p.railOn = 0;
    for (size_t i = 1; i < p.rail.size(); ++i) {
        const Link& k = p.rail[i].link;
        if ((go == Go::Quests && k.go == Go::Quests) || (section >= 0 && k.go == Go::Section && k.id == section))
            p.railOn = int(i);
    }
    return p;
}

std::string titleOf(const refindex::Index& ix, Lang l, const Link& link) {
    switch (link.go) {
        case Go::Mob: return ix.mobs.count(link.id) ? ix.mobs.at(link.id).name : "";
        case Go::Item: return ix.items.count(link.id) ? ix.items.at(link.id).name : "";
        case Go::Map: return link.map.empty() ? "" : refindex::mapTitle(ix, link.map);
        case Go::Quest:
            return link.id >= 0 && size_t(link.id) < ix.quests.size()
                       ? refquests::textOf(ix.quests[size_t(link.id)].title, questLang(l))
                       : "";
        default: return "";
    }
}

bool sameCard(const Link& a, const Link& b) { return a.go == b.go && a.id == b.id && a.map == b.map; }

std::string linkText(const Link& link) {
    switch (link.go) {
        case Go::Mob: return "m " + std::to_string(link.id);
        case Go::Item: return "i " + std::to_string(link.id);
        case Go::Map: return "p " + link.map;
        case Go::Quest: return "q " + std::to_string(link.id);
        default: return "";
    }
}

bool linkOf(const std::string& line, Link& out) {
    if (line.size() < 3 || line[1] != ' ') return false;
    const std::string rest = line.substr(2);
    switch (line[0]) {
        case 'm': out = Link{Go::Mob, std::atoi(rest.c_str()), ""}; return out.id > 0;
        case 'i': out = Link{Go::Item, std::atoi(rest.c_str()), ""}; return out.id > 0;
        case 'q': out = Link{Go::Quest, std::atoi(rest.c_str()), ""}; return out.id >= 0;
        case 'p': {
            for (char ch : rest)
                if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_' && ch != '-') return false;
            out = Link{Go::Map, 0, rest};
            return true;
        }
        default: return false;
    }
}

namespace {

Page card(const refindex::Index& ix, Lang l, const Link& link, const Context& c, unsigned opened) {
    switch (link.go) {
        case Go::Mob: return mob(ix, l, link.id, c, opened);
        case Go::Item: return item(ix, l, link.id, c, opened);
        case Go::Map: return map(ix, l, link.map, c, opened);
        case Go::Shop: return shop(ix, l, link.id);
        case Go::Section: return section(ix, l, link.id, c);
        case Go::World: return link.id == 1 ? dungeons(ix, l, c) : world(ix, l, c, link.map);
        case Go::Quests: return quests(ix, l);
        case Go::Quest: return quest(ix, l, link.id);
        case Go::Fold:
        case Go::Star:
        case Go::None:
        case Go::Expand:
        case Go::Follow: break;
    }
    return home(ix, l);
}

}  // namespace

}  // namespace uagfx::refpages
