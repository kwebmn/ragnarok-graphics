#include "win/minimap_proto.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/layout.hpp"
#include "core/minimap_proto.hpp"
#include "core/pack_source.hpp"
#include "core/reference_index.hpp"
#include "core/reference_live.hpp"
#include "core/reference_pages.hpp"
#include "core/route_follow.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "core/walk_grid.hpp"
#include "core/walk_path.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/ground_way.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/map_hd.hpp"
#include "win/party_panel.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/reference_window.hpp"
#include "win/ui_cost.hpp"

namespace uagfx::win::minimapproto {
namespace {

const std::string kUi = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";  // 유저인터페이스
constexpr size_t kMostLines = 8;            // 3.0.11-beta.6: the monsters under the map at most (the last «і ще N»)
constexpr int kBossRespawnMs = 600000;      // a boss's respawn told from 10 minutes (the site's MVP timers' bound)
constexpr uint32_t kBossText = 0xF5C451;    // a boss's name
constexpr ULONGLONG kThereMs = 8000;  // 3.0.11-beta.9: «Ви на місці» shown so long, then the way ends

struct Line {  // a monster under the map: its name, its count (a boss's respawn)
    std::string left, right;
    bool boss = false;
    int mob = 0;         // 3.0.11-beta.9: its id - a click opens its card (0: «і ще N», the map's card)
    bool timer = false;  // 3.0.11-beta.12: the right words a player's public timer (its colour)
};

struct State {
    bool on = false, shown = false, dirty = true, upload = true;
    int style = -1;  // 3.0.11-beta.47: core/ui_style's generation painted
    bool placedMoved = false;  // 3.0.11-beta.79: moved by the player when last laid out
    minimap::Look look = minimap::Look::Opaque;  // 3.0.11-beta.4: Ctrl+Tab's round
    bool hidOld = false;      // the game's own window 14 deleted by us (the game makes it again on a new map)
    bool hideFailed = false;  // told once a map
    std::string map;
    // the map's own minimap picture (the game's), the whole map. 3.0.11-beta.72 (the owner: «сами картинки карт
    // маленькие и не аккуратные», «розовые полосы по краям»): prepared (core minimap::prepare - its pinkish edges
    // see-through, halved down level by level), and four times bigger once found (win/map_hd, [interface] minimap_hd)
    minimap::MapPicture prepared;
    int picW = 0, picH = 0;
    std::vector<uint8_t> bmp;  // its file: the AI pack's keys
    std::shared_ptr<const minimap::MapPicture> hd;
    bool hdOn = true, hdAsked = false;
    minimap::MapSize size;
    minimap::Cell cell;
    bool cellKnown = false;
    int clicks = 0;  // kept from the game on this map
    int remade = 0;  // 3.0.11-beta.4: the game made its own again on this map after we hid it (a Ctrl+Tab it saw)
    minimap::Layout layout;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0, targetW = 0, targetH = 0;
    float k = 0, scale = 1;
    std::unique_ptr<gamefiles::Reader> files;
    // 3.0.11-beta.6: the hints - from this reference's map, as the switches want them
    Show show;
    std::shared_ptr<const refindex::Index> index;
    std::string hintsMap;
    bool hintsMade = false;
    std::vector<minimap::Mark> marks;
    std::vector<Line> lines;
    int hover = -1;                              // the mark under the pointer
    minimap::Part part = minimap::Part::None;    // 3.0.11-beta.31: the panel's part under it
    std::map<std::string, std::string> mapNames;  // the client's data\mapnametable.txt
    bool namesRead = false;
    // 3.0.11-beta.12: the players' public MVP timers - the bosses' lines say when, made again each minute
    std::shared_ptr<const reflive::Live> live;
    ULONGLONG hintsAt = 0;
    // 3.0.11-beta.9: the way followed - its lines first under the map
    routefollow::Follow follow;
    ULONGLONG arrivedAt = 0;
    std::vector<minimap::Line> routeLines;
    // 3.0.11-beta.31 (the owner: «нету кнопочок + и -», «описание под картой надо иметь возможность сворачивать»):
    // the zoom and the details folded, kept in a file; where the camera looks; the way over the walked cells
    int zoom = minimap::kZoomFirst;
    bool folded = false, stateRead = false;
    float lookX = 0, lookY = 0;
    std::string cellsMap;  // the walked cells known for (the reference's walkable.bin)
    walkpath::Cells cells;
    walkpath::Field field;
    std::vector<minimap::Cell> path;
    minimap::Cell pathFrom{-1, -1};
    // 3.0.11-beta.34 (the owner: «каждый шаг перестраивается маршрут»): the way kept while the player keeps to it
    walkpath::Kept kept;
    int keptSteps = 0;  // the player's cells on it since it was made
    int keptLogged = 0;  // on this map
    // 3.0.11-beta.31 (the owner: «нету возможности карту развернуть на весь екран»): the map big in the middle
    bool big = false, bigDirty = true, bigUpload = true;
    minimap::Big bigLayout;
    minimap::BigPart bigPart = minimap::BigPart::None;
    panel::Picture bigMap, bigPicture;  // its map's picture alone (made once a map and a size), and all of it
    std::string bigMapOf;
    uint32_t bigTexture = 0;
    int bigTexW = 0, bigTexH = 0;
    // 3.0.11-beta.57 (R-T942DZ: in Prontera 108 paintings in 30 s, 183 on prt_fild08 - each step and each degree
    // the camera turned): painted again at most each kPaintEveryMs, what changed meanwhile in the one after
    ULONGLONG paintedAt = 0, bigPaintedAt = 0;
    // 3.0.11-beta.65 (the concept's «Подсветка, куда можно дойти», an experiment): the cells reached on foot from the
    // player - their part of the map (walkgrid), made again when the player is in another part or on another map
    std::vector<uint8_t> reach;
    std::string reachMap;
    uint8_t reachPart = walkgrid::kWall;
    int reachLogged = 0;
    // 3.0.11-beta.71 (the owner: «иконка игрока не поворачивается в сторону куда идешь», «туда куда чар смотрит»): the
    // character's facing (its object + 0x38) - the arrow's way
    float facing = 1e9f, headX = 0, headY = 0;
    int facingLogged = 0;
    // 3.0.11-beta.71 (the owner: «хочется на карте прокладывать путь»): the points set on the map by clicks (Shift adds
    // one), the way through them from the first (walkpath::through), its lines under the map; reached - «Ви на місці»
    std::vector<minimap::Cell> plot;
    std::string plotMap;
    std::vector<walkpath::Cell> plotTail;  // from the first point through the rest
    std::vector<minimap::Line> plotLines;
    ULONGLONG plotThereAt = 0;
    // 3.0.11-beta.83 (the owner's 3): the party's members on this map - their faces at their cells
    std::vector<minimap::Mark> members;
    std::string membersKey;
};

constexpr ULONGLONG kPaintEveryMs = 33;

State& state() {
    static State s;
    return s;
}

void loadPicture(State& s) {
    s.prepared = minimap::MapPicture{};
    s.picW = s.picH = 0;
    s.bmp.clear();
    s.hd.reset();
    s.hdAsked = false;
    if (!s.files) s.files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    std::vector<uint32_t> argb;
    std::string from;
    const std::string entry = "data\\texture\\" + kUi + "\\map\\" + s.map + ".bmp";
    if (s.files->read(entry, bytes, &from) && pack::decodeBmp(bytes.data(), bytes.size(), argb, s.picW, s.picH)) {
        s.prepared = minimap::prepare(argb, s.picW, s.picH);
        s.bmp = std::move(bytes);
        LOG_INFO("minimap: %s's picture %dx%d from %s", s.map.c_str(), s.picW, s.picH, from.c_str());
        return;
    }
    s.picW = s.picH = 0;
    LOG_INFO("minimap: %s has no picture of its own - the ground alone", s.map.c_str());
}

// 3.0.11-beta.6: the client's names of maps (data\mapnametable.txt) for the warps' words, read once
void readNames(State& s) {
    if (s.namesRead) return;
    s.namesRead = true;
    if (!s.files) s.files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    if (s.files->read("data\\mapnametable.txt", bytes, nullptr))
        s.mapNames = refindex::mapNames(std::string(bytes.begin(), bytes.end()));
    LOG_INFO("minimap: %zu names of maps from the client's mapnametable.txt", s.mapNames.size());
}

std::string say(settings::Text t) { return settings::tr(startup().lang, t); }

// 3.0.11-beta.9: a map's short name: the client's, else its code
std::string shortTitle(State& s, const std::string& code) {
    readNames(s);
    const auto name = s.mapNames.find(code);
    const std::string short_ = name == s.mapNames.end() ? "" : refindex::shortName(name->second);
    return short_.empty() ? code : short_;
}

// 3.0.11-beta.31: the zoom and the details' fold kept for the next games
void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    if (!readTextFile(layout::join(gameDir(), layout::kMinimapState), text)) return;
    int zoom = s.zoom, folded = 0;
    if (const size_t at = text.find("zoom "); at != std::string::npos) zoom = std::atoi(text.c_str() + at + 5);
    if (const size_t at = text.find("folded "); at != std::string::npos) folded = std::atoi(text.c_str() + at + 7);
    s.zoom = std::clamp(zoom, 0, minimap::kZoomSteps - 1);
    s.folded = folded != 0;
}

void keepState(const State& s) {
    const std::string path = layout::join(gameDir(), layout::kMinimapState);
    const std::string text = "zoom " + std::to_string(s.zoom) + "\nfolded " + (s.folded ? "1" : "0") + "\n";
    if (!writeFileAtomic(path, text)) LOG_INFO("minimap: %s cannot be written", path.c_str());
}

// 3.0.11-beta.6: the map's hints from the reference - marks with their words, the monsters' lines
void buildHints(State& s) {
    s.marks.clear();
    s.lines.clear();
    s.hover = -1;
    s.hintsMap = s.map;
    s.hintsMade = true;
    const refindex::MapHints* h = nullptr;
    if (s.index) {
        const auto it = s.index->maps.find(s.map);
        if (it != s.index->maps.end()) h = &it->second;
    }
    if (h && s.show.npcs) {
        for (const refindex::Npc& n : h->npcs)
            s.marks.push_back(minimap::Mark{minimap::Cell{n.x, n.y}, refindex::colorOf(n.kind),
                                            n.kind == refindex::NpcKind::Kafra ? minimap::Glyph::Kafra
                                                                               : minimap::Glyph::Dot,
                                            n.name});
    }
    if (h && s.show.warps) {
        readNames(s);
        for (const refindex::Warp& w : h->warps) {
            bool twin = false;  // the same warp's second cell beside it (the server's warps are 2 cells wide)
            for (const minimap::Mark& m : s.marks)
                twin = twin || (m.glyph == minimap::Glyph::Warp && std::abs(m.cell.x - w.x) <= 2 &&
                                std::abs(m.cell.y - w.y) <= 2);
            if (twin) continue;
            s.marks.push_back(minimap::Mark{minimap::Cell{w.x, w.y}, 0, minimap::Glyph::Warp,
                                            "\xE2\x86\x92 " + shortTitle(s, w.dest)});  // «→ Prontera Field»
        }
    }
    if (!s.show.mobs) return;
    if (!s.index) {
        s.lines.push_back(Line{say(settings::Text::MinimapNoReference), "", false});
        return;
    }
    if (!h || h->mobs.empty()) {
        s.lines.push_back(Line{say(settings::Text::MinimapNoMonsters), "", false});
        return;
    }
    const size_t room = h->mobs.size() > kMostLines ? kMostLines - 1 : h->mobs.size();
    for (size_t i = 0; i < room; ++i) {
        const refindex::MobOnMap& m = h->mobs[i];
        const auto mob = s.index->mobs.find(m.id);
        const std::string name = mob != s.index->mobs.end() ? mob->second.name : "#" + std::to_string(m.id);
        const bool boss = mob != s.index->mobs.end() && mob->second.boss;
        std::string right = std::to_string(m.count);
        if (boss && m.respawnMs >= kBossRespawnMs) right = refpages::respawn(m.respawnMs, startup().lang);
        bool timer = false;  // 3.0.11-beta.12: a player's public timer of it here - when it comes back
        if (boss && s.live && m.respawnMs > 0) {
            const reflive::Timer* t = nullptr;
            for (const reflive::Timer& o : s.live->timers)  // the latest first
                if (o.mob == m.id && o.map == s.map) {
                    t = &o;
                    break;
                }
            if (t) {
                const long long left = t->killed + m.respawnMs / 1000 - static_cast<long long>(std::time(nullptr));
                right = left > 0 ? refpages::respawn(int(std::max<long long>(left, 60) * 1000), startup().lang)
                                 : say(settings::Text::MinimapTimerDue);
                timer = true;
            }
        }
        s.lines.push_back(Line{(boss ? "\xE2\x98\x85 " : "") + name, right, boss, m.id, timer});
    }
    if (room < h->mobs.size())
        s.lines.push_back(Line{settings::fmt(say(settings::Text::MinimapMore), {std::to_string(h->mobs.size() - room)}),
                               "", false});
}

void buildRoute(State& s) {
    s.routeLines.clear();
    if (!s.follow.active) return;
    const std::string to = shortTitle(s, s.follow.target);
    if (s.map == s.follow.target) {
        s.routeLines.push_back(minimap::Line{say(settings::Text::MinimapRouteThere), ""});
        s.routeLines.push_back(minimap::Line{to, ""});
        return;
    }
    const refroute::Step* step = routefollow::next(s.follow, s.map);
    if (!step) {
        s.routeLines.push_back(minimap::Line{say(settings::Text::MinimapRouteNone), ""});
        s.routeLines.push_back(minimap::Line{"\xE2\x86\x92 " + to, ""});
        return;
    }
    const std::string left =
        settings::fmt(say(settings::Text::MinimapRouteLeft), {std::to_string(s.follow.route.steps.size())});
    s.routeLines.push_back(minimap::Line{"\xE2\x86\x92 " + shortTitle(s, step->move.to), left});
    s.routeLines.push_back(minimap::Line{refpages::stepNote(*step, startup().lang), ""});
}

// 3.0.11-beta.31: the way on the ground itself (win/ground_way) - the same cells as the minimap's line.
// 3.0.11-beta.34: the way kept, whole - the ground cuts it at the player's place itself (not their cell)
void toGround(const State& s) {
    std::vector<std::pair<int, int>> cells;
    if (s.path.size() >= 2) cells = s.kept.way.size() >= 2 ? s.kept.way : std::vector<std::pair<int, int>>{};
    if (!cells.empty() && !s.plot.empty() && s.plotTail.size() > 1)  // 3.0.11-beta.71: through the points set
        cells.insert(cells.end(), s.plotTail.begin() + 1, s.plotTail.end());
    win::groundway::set(s.map, cells);
}

// 3.0.11-beta.31: the way over the walked cells to the next move's cell - its distances made once a goal, its cells
// from the player each time the player is on another cell; no cells known for the map: a straight line.
// 3.0.11-beta.34 (the owner: «каждый шаг перестраивается маршрут», «каждый раз перестраивается линия, можно так чтобы
// она не очень дергалась»): the way kept while the player keeps to it (walkpath::keep) - the minimap's line cut at
// them, made again only for another goal or the player 5 cells off it (the game walks to a click diagonal first, then
// straight: off a long leg by a fifth of it - offline, a click 8 to 21 cells ahead along the way: 3 cells made it again
// 15 times in 40 walks, 5 never)
constexpr float kOffCells = 5;

// The walked cells of the player's map (the reference's walkable.bin), read once a map
void loadCells(State& s) {
    if (s.cellsMap == s.map) return;
    s.cellsMap = s.map;
    s.cells = walkpath::Cells{};
    s.field = walkpath::Field{};
    s.kept = walkpath::Kept{};
    s.keptLogged = 0;
    if (s.index && s.index->walk) {
        const auto it = s.index->walk->find(s.map);
        if (it != s.index->walk->end()) s.cells = walkpath::cellsOf(it->second);
    }
}

void makePath(State& s) {
    // 3.0.11-beta.71: the points set on the map first - the way to the first, then through the rest
    const bool plotted = !s.plot.empty() && s.plotMap == s.map;
    const refroute::Step* step = !plotted && s.cellKnown ? routefollow::next(s.follow, s.map) : nullptr;
    if ((!plotted && !step) || !s.cellKnown) {
        if (!s.path.empty()) s.dirty = true;
        s.path.clear();
        s.kept = walkpath::Kept{};
        toGround(s);
        return;
    }
    const minimap::Cell goal = plotted ? s.plot.front() : minimap::Cell{step->move.x, step->move.y};
    loadCells(s);
    const bool newGoal = s.field.goal != walkpath::Cell{goal.x, goal.y} || s.field.w != s.cells.w;
    if (newGoal) {  // its distances (none with no cells known: the goal alone, the way straight to it)
        const ULONGLONG t0 = GetTickCount64();
        s.field = walkpath::fieldTo(s.cells, walkpath::Cell{goal.x, goal.y});
        s.kept = walkpath::Kept{};
        if (s.cells.w > 0)
            LOG_INFO("route: the walked cells of %s to %d, %d measured in %llu ms", s.map.c_str(), goal.x, goal.y,
                     static_cast<unsigned long long>(GetTickCount64() - t0));
    }
    if (!newGoal && s.pathFrom.x == s.cell.x && s.pathFrom.y == s.cell.y) return;
    s.pathFrom = s.cell;
    const float wasOff = s.kept.off, wasAt = s.kept.at;
    const bool had = s.kept.way.size() >= 2;
    if (walkpath::keep(s.kept, s.field, s.cells, walkpath::Cell{s.cell.x, s.cell.y}, kOffCells)) {
        if (s.keptLogged < 12) {
            ++s.keptLogged;
            if (had)
                LOG_INFO("route: the way on %s made again from %d, %d - %zu corners (the one before kept %d steps, "
                         "%.1f cells along; the player off it by more than %.0f cells, %.1f the step before)",
                         s.map.c_str(), s.cell.x, s.cell.y, s.kept.way.size(), s.keptSteps, wasAt, kOffCells, wasOff);
            else
                LOG_INFO("route: the way on %s made from %d, %d - %zu corners%s", s.map.c_str(), s.cell.x, s.cell.y,
                         s.kept.way.size(), s.cells.w > 0 ? "" : " (no walked cells known: straight to the goal)");
        }
        s.keptSteps = 0;
    } else if (++s.keptSteps == 10 && s.keptLogged < 12) {
        ++s.keptLogged;
        LOG_INFO("route: the way on %s kept 10 steps so far - cut at the player, not made again (%.1f cells along it, "
                 "%.1f off it)", s.map.c_str(), s.kept.at, s.kept.off);
    }
    s.path.clear();
    for (const walkpath::Cell& c : s.kept.rest) s.path.push_back(minimap::Cell{c.first, c.second});
    if (plotted)  // 3.0.11-beta.71: on through the rest of the points
        for (size_t i = 1; i < s.plotTail.size(); ++i)
            s.path.push_back(minimap::Cell{s.plotTail[i].first, s.plotTail[i].second});
    s.dirty = true;
    s.bigDirty = true;
    toGround(s);
}

// 3.0.11-beta.71 (the owner: «хочется на карте прокладывать путь»): the way through the points set - its tail from the
// first point through the rest (made once a set), its two lines under the map (the goal, how far; the keys)
void plotTail(State& s) {
    loadCells(s);
    s.plotTail.clear();
    if (s.plot.size() > 1) {
        std::vector<walkpath::Cell> rest;
        for (size_t i = 1; i < s.plot.size(); ++i) rest.emplace_back(s.plot[i].x, s.plot[i].y);
        s.plotTail = walkpath::through(s.cells, walkpath::Cell{s.plot[0].x, s.plot[0].y}, rest);
    }
}

void plotLines(State& s) {
    s.plotLines.clear();
    if (s.plotThereAt) {
        s.plotLines.push_back(minimap::Line{say(settings::Text::MinimapRouteThere), ""});
        return;
    }
    if (s.plot.empty()) return;
    float cells = 0;  // along the way
    for (size_t i = 1; i < s.path.size(); ++i)
        cells += std::hypot(float(s.path[i].x - s.path[i - 1].x), float(s.path[i].y - s.path[i - 1].y));
    const minimap::Cell& end = s.plot.back();
    s.plotLines.push_back(minimap::Line{
        settings::fmt(say(settings::Text::MinimapPlotTo), {std::to_string(end.x), std::to_string(end.y)}),
        s.path.size() > 1 ? settings::fmt(say(settings::Text::MinimapPlotCells), {std::to_string(int(cells + 0.5f))})
                          : ""});
    s.plotLines.push_back(minimap::Line{say(settings::Text::MinimapPlotHint), ""});
}

void clearPlot(State& s) {
    if (s.plot.empty() && !s.plotThereAt) return;
    s.plot.clear();
    s.plotTail.clear();
    s.plotLines.clear();
    s.plotThereAt = 0;
    s.kept = walkpath::Kept{};
    s.field = walkpath::Field{};
    s.pathFrom = minimap::Cell{-1, -1};
    s.path.clear();
    s.dirty = s.bigDirty = true;
    toGround(s);
    LOG_INFO("route: the way set on the map taken away");
}

// A click's cell: the way to it (Shift: one more point after those set, at most 8)
void setPlot(State& s, const minimap::Cell& at, bool add) {
    if (!add || s.plotMap != s.map || s.plotThereAt) s.plot.clear();
    if (s.plot.size() >= 8) s.plot.pop_back();
    s.plot.push_back(at);
    s.plotMap = s.map;
    s.plotThereAt = 0;
    s.field = walkpath::Field{};  // the first point maybe another: its distances made again
    s.kept = walkpath::Kept{};
    s.pathFrom = minimap::Cell{-1, -1};
    plotTail(s);
    LOG_INFO("route: a way set on %s to %d, %d (%zu points; from %d, %d)", s.map.c_str(), at.x, at.y, s.plot.size(),
             s.cell.x, s.cell.y);
}

// Each frame while points are set: the first one reached (a cell away) - the next; the last - «Ви на місці» a while;
// another map - gone. True: the way set is the one shown (the followed way waits)
bool plotFrame(State& s) {
    if (s.plot.empty() && !s.plotThereAt) return false;
    const ULONGLONG now = GetTickCount64();
    if (s.plotMap != s.map) {
        clearPlot(s);
        return false;
    }
    if (s.plotThereAt) {
        if (now - s.plotThereAt > kThereMs) {
            clearPlot(s);
            return false;
        }
        return true;
    }
    // reached: a cell from the point, or at the end of the walked way to it (a point on a wall: its ground beside it)
    const auto within = [&](int x, int y) { return std::max(std::abs(s.cell.x - x), std::abs(s.cell.y - y)) <= 1; };
    const bool atEnd = !s.kept.rest.empty() && s.kept.goal == walkpath::Cell{s.plot.front().x, s.plot.front().y} &&
                       within(s.kept.rest.back().first, s.kept.rest.back().second);
    if (s.cellKnown && (within(s.plot.front().x, s.plot.front().y) || atEnd)) {
        s.plot.erase(s.plot.begin());
        s.field = walkpath::Field{};
        s.kept = walkpath::Kept{};
        s.pathFrom = minimap::Cell{-1, -1};
        if (s.plot.empty()) {
            s.plotThereAt = now;
            s.path.clear();
            toGround(s);
            plotLines(s);
            s.dirty = s.bigDirty = true;
            LOG_INFO("route: the way set on %s reached at %d, %d", s.map.c_str(), s.cell.x, s.cell.y);
            return true;
        }
        plotTail(s);
    }
    makePath(s);
    plotLines(s);
    return true;
}

// The way followed now on this map: made again when the player went elsewhere; «Ви на місці» a while, then the end
void followFrame(State& s) {
    if (!s.follow.active || !s.index) {
        if (!s.path.empty()) {
            s.path.clear();
            s.dirty = s.bigDirty = true;
            toGround(s);
        }
        return;
    }
    bool changed = routefollow::update(s.follow, *s.index, s.map, s.cellKnown ? s.cell.x : -1,
                                       s.cellKnown ? s.cell.y : -1);
    if (changed) {
        const refroute::Step* step = routefollow::next(s.follow, s.map);
        LOG_INFO("route: to %s from %s at %d, %d - %s", s.follow.target.c_str(), s.map.c_str(), s.cell.x, s.cell.y,
                 step ? (std::to_string(s.follow.route.steps.size()) + " moves, the next to " + step->move.to + " at " +
                         std::to_string(step->move.x) + ", " + std::to_string(step->move.y)).c_str()
                      : s.map == s.follow.target ? "there" : "no way from here");
    }
    if (s.map == s.follow.target) {
        const ULONGLONG now = GetTickCount64();
        if (!s.arrivedAt) {
            s.arrivedAt = now;
            changed = true;
        } else if (now - s.arrivedAt > kThereMs) {
            LOG_INFO("route: there (%s) - the way ends", s.follow.target.c_str());
            s.follow = routefollow::Follow{};
            changed = true;
        }
    }
    if (changed) {
        buildRoute(s);
        s.dirty = s.bigDirty = true;
    }
    makePath(s);
}

// 3.0.11-beta.31: where the camera looks along the ground - its view's forward (the matrix's third column: world ->
// view for row vectors) east (x) and north (z)
bool readLook(float& east, float& north) {
    float view[12];
    if (!readGameCamera(view)) return false;
    const float x = view[2], z = view[8], len = std::hypot(x, z);
    if (len < 1e-3f) return false;
    east = x / len;
    north = z / len;
    return true;
}

// 3.0.11-beta.65 (the concept's «Подсветка, куда можно дойти» - an experiment, [interface] minimap_reach): the cells
// of the player's part of the map (walkgrid - the server's own .gat, parts joined across no wall) left lit, the rest
// dimmed; made again only when the player is in another part (a teleport, a warp, a map) - a step changes nothing.
// None while the player's cell is not known, the map not in walkable.bin, or the cell on a wall between two parts
void reachFrame(State& s, bool on) {
    uint8_t part = walkgrid::kWall;
    const walkgrid::Grid* grid = nullptr;
    if (on && s.cellKnown && s.index && s.index->walk) {
        const auto it = s.index->walk->find(s.map);
        if (it != s.index->walk->end() && it->second.w == s.size.w && it->second.h == s.size.h) {
            grid = &it->second;
            part = walkgrid::partOf(*grid, s.cell.x, s.cell.y);
        }
    }
    if (part == walkgrid::kWall) {
        // on a wall a moment (the game's cell a step behind the server's): the part kept on the same map
        if (on && grid && s.reachMap == s.map && !s.reach.empty()) return;
        if (!s.reach.empty()) {
            s.reach.clear();
            s.dirty = s.bigDirty = true;
        }
        s.reachMap.clear();
        s.reachPart = walkgrid::kWall;
        return;
    }
    if (s.reachMap == s.map && s.reachPart == part && !s.reach.empty()) return;
    const ULONGLONG t0 = GetTickCount64();
    s.reach = walkgrid::partMask(*grid, part);
    s.reachMap = s.map;
    s.reachPart = part;
    s.dirty = s.bigDirty = true;
    if (s.reachLogged++ < 20) {
        const size_t lit = size_t(std::count(s.reach.begin(), s.reach.end(), uint8_t(1)));
        LOG_INFO("minimap: where one can walk on %s from %d, %d - part %d of %d, %zu of %dx%d cells (%llu ms)",
                 s.map.c_str(), s.cell.x, s.cell.y, int(part), grid->partCount, lit, grid->w, grid->h,
                 static_cast<unsigned long long>(GetTickCount64() - t0));
    }
}

// 3.0.11-beta.72: the map's picture shown - four times bigger once found ([interface] minimap_hd), else the game's
const minimap::MapPicture* shownPicture(const State& s) {
    const minimap::MapPicture* p = s.hdOn && s.hd ? s.hd.get() : &s.prepared;
    return p->levels.empty() ? nullptr : p;
}

// 3.0.11-beta.72: asked once a map with the switch on, shown when the thread has it (off: the game's at once)
void hdFrame(State& s, bool on) {
    if (on != s.hdOn) {
        s.hdOn = on;
        s.dirty = s.bigDirty = true;
        LOG_INFO("minimap: the maps' bigger pictures %s ([interface] minimap_hd)", on ? "on" : "off");
    }
    if (!on) return;
    if (!s.hdAsked) {
        s.hdAsked = true;
        maphd::ask(s.map, s.bmp);
    }
    if (std::shared_ptr<const minimap::MapPicture> p = maphd::take(s.map)) {
        s.hd = std::move(p);
        s.dirty = s.bigDirty = true;
    }
}

minimap::Scene sceneOf(const State& s) {
    minimap::Scene sc;
    sc.picture = shownPicture(s);  // 3.0.11-beta.72 (no argb: never prepared again at each painting)
    sc.map = s.size;
    sc.player = s.cell;
    sc.lookX = s.lookX;
    sc.lookY = s.lookY;
    sc.cells = minimap::cellsAt(s.zoom, s.size);
    sc.marks = s.marks;
    sc.marks.insert(sc.marks.end(), s.members.begin(), s.members.end());  // 3.0.11-beta.83: after the hints
    sc.path = s.path;
    if (!s.plot.empty() && s.plotMap == s.map) {  // 3.0.11-beta.71: the points set - the last a pin
        sc.pin = true;
        sc.pinCell = s.plot.back();
        sc.waypoints.assign(s.plot.begin(), s.plot.end() - 1);
    } else if (const refroute::Step* step = s.cellKnown ? routefollow::next(s.follow, s.map) : nullptr) {
        sc.pin = true;
        sc.pinCell = minimap::Cell{step->move.x, step->move.y};
    }
    sc.headX = s.headX;  // 3.0.11-beta.71: the character's facing
    sc.headY = s.headY;
    sc.hoverMark = s.hover;
    sc.hover = s.part;
    sc.bigHover = s.bigPart;
    if (!s.reach.empty()) sc.reach = &s.reach;  // 3.0.11-beta.65
    return sc;  // see-through (Ctrl+Tab) with its words: render() fades the whole picture
}

std::vector<Words> gdiWords(const std::vector<minimap::Words>& in) {
    std::vector<Words> out;
    for (const minimap::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == minimap::Align::Right    ? DT_RIGHT
                                 : w.align == minimap::Align::Center ? DT_CENTER
                                                                     : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    minimap::Texts t;
    t.name = shortTitle(s, s.map);
    if (s.cellKnown) t.cell = std::to_string(s.cell.x) + ", " + std::to_string(s.cell.y);
    t.folded = s.folded;
    t.route = !s.plotLines.empty() ? s.plotLines : s.routeLines;  // 3.0.11-beta.71: the way set on the map first
    for (const Line& l : s.lines)
        t.list.push_back(minimap::Line{l.left, l.right, l.boss ? kBossText : 0u,
                                       l.timer ? minimap::kRouteRgb : 0u, l.boss});
    s.layout = minimap::layout(s.targetW, s.targetH, s.scale, !t.route.empty(), s.folded ? 0 : int(t.list.size()));
    hudedit::place(hudlayout::Part::Minimap, s.k, s.targetW, s.targetH, s.layout.w, s.layout.h, s.layout.x,
                   s.layout.y);  // 3.0.11-beta.75: where the player put it
    const uicost::Paint cost("minimap", s.layout.w, s.layout.h);  // 3.0.11-beta.55 (beta.57: its size known)
    s.paintedAt = GetTickCount64();
    const minimap::Scene sc = sceneOf(s);
    minimap::paint(s.picture, s.layout, sc, t);
    drawWords(s.picture, gdiWords(minimap::words(s.layout, sc, t)));
    if (s.look == minimap::Look::Faded) panel::fade(s.picture, minimap::opacity(s.look));  // Ctrl+Tab's see-through
    s.dirty = false;
    s.upload = true;
}

void renderBig(State& s) {
    const minimap::Scene sc = sceneOf(s);
    s.bigLayout = minimap::bigLayout(s.targetW, s.targetH, s.scale, s.size);
    const uicost::Paint cost("big map", s.bigLayout.w, s.bigLayout.h);  // 3.0.11-beta.55 (beta.57: its size known)
    s.bigPaintedAt = GetTickCount64();
    const int picture = sc.picture ? sc.picture->levels.front().w : 0;  // 3.0.11-beta.72: the game's or the bigger
    const std::string key = s.map + "|" + std::to_string(s.bigLayout.w) + "x" + std::to_string(s.bigLayout.h) + "|" +
                            std::to_string(picture) + "|" + std::to_string(s.reach.empty() ? -1 : int(s.reachPart));
    if (key != s.bigMapOf) {  // the map's picture at its size: made once
        s.bigMapOf = key;
        minimap::paintBigMap(s.bigMap, s.bigLayout, sc);
    }
    minimap::paintBig(s.bigPicture, s.bigMap, s.bigLayout, sc);
    const std::string cell = s.cellKnown ? std::to_string(s.cell.x) + ", " + std::to_string(s.cell.y) : "";
    drawWords(s.bigPicture, gdiWords(minimap::bigWords(s.bigLayout, sc, shortTitle(s, s.map) + " \xC2\xB7 " + s.map,
                                                       cell)));
    s.bigDirty = false;
    s.bigUpload = true;
}

bool toTarget(const State& s, HWND game, int x, int y, float& tx, float& ty) {
    RECT client{};
    if (!GetClientRect(game, &client) || client.right <= 0 || client.bottom <= 0) return false;
    tx = float(x) * float(s.targetW) / float(client.right);  // Windows' size of the window (uaRO.exe is not DPI-aware)
    ty = float(y) * float(s.targetH) / float(client.bottom);
    return true;
}

RECT clientRect(const State& s, HWND game, int x, int y, int w, int h) {
    RECT client{};
    GetClientRect(game, &client);
    return RECT{LONG(int64_t(x) * client.right / s.targetW), LONG(int64_t(y) * client.bottom / s.targetH),
                LONG(int64_t(x + w) * client.right / s.targetW + 1),
                LONG(int64_t(y + h) * client.bottom / s.targetH + 1)};
}

void block(const State& s, HWND game) {  // the mouse's clicks there are the panel's, not the game's
    if (s.targetW <= 0 || s.targetH <= 0) return;
    const RECT r = clientRect(s, game, s.layout.x, s.layout.y, s.layout.w, s.layout.h);
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Minimap, game, &r, false);
    if (s.big) {
        const RECT b = clientRect(s, game, s.bigLayout.x, s.bigLayout.y, s.bigLayout.w, s.bigLayout.h);
        uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::BigMap, game, &b, false);
    } else {
        uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::BigMap, nullptr, nullptr, false);
    }
}

void unblock() {
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Minimap, nullptr, nullptr, false);
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::BigMap, nullptr, nullptr, false);
}

void setBig(State& s, bool open) {
    if (s.big == open) return;
    s.big = open;
    s.bigDirty = true;
    s.bigPaintedAt = 0;  // 3.0.11-beta.57: opened - painted at once
    s.bigPart = minimap::BigPart::None;
    LOG_INFO("minimap: the big map %s", open ? "opened" : "closed");
}

void setZoom(State& s, int step) {
    step = std::clamp(step, 0, minimap::kZoomSteps - 1);
    if (step == s.zoom) return;
    s.zoom = step;
    s.dirty = true;
    keepState(s);
}

}  // namespace

void frame(HWND game, const Show& show, double k, int targetWidth, int targetHeight) {
    State& s = state();
    const bool on = show.on;
    win::groundway::look(show.way);  // 3.0.11-beta.33: footprints, the strip or none
    if (!on) {
        if (s.shown) unblock();
        s.shown = false;
        s.big = false;
        win::groundway::set("", {});  // 3.0.11-beta.31: no way on the ground either
        if (s.on) {  // switched off: the game's own back where the map has one
            s.on = false;
            const bool back = s.hidOld && showGameMinimap();
            LOG_INFO("minimap: the prototype off - the game's own %s",
                     back ? "made again" : "left to the game (the map has none, or it was never hidden)");
            s.hidOld = false;
            s.map.clear();
        }
        return;
    }
    if (!s.on) {
        s.on = true;
        LOG_INFO("minimap: the prototype on ([interface] minimap = 1)");
    }
    readState(s);
    const std::string map = readGameMap();
    if (!game || map.empty() || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {  // the login, the characters
        if (s.shown) unblock();
        s.shown = false;
        return;
    }
    if (map != s.map) {
        s.map = map;
        s.clicks = 0;
        s.remade = 0;
        s.cellKnown = false;
        s.hideFailed = false;
        s.pathFrom = minimap::Cell{-1, -1};
        s.lookX = s.lookY = 0;
        s.facingLogged = std::min(s.facingLogged, 4);  // 3.0.11-beta.71: a line or two on each map
        loadPicture(s);
        s.dirty = s.bigDirty = true;
        s.paintedAt = s.bigPaintedAt = 0;  // 3.0.11-beta.57: another map's at once, not the last one's a while
    }
    hdFrame(s, show.hd);  // 3.0.11-beta.72
    // the game's own minimap, each time the game makes it again (a new map, Ctrl+Tab): hidden as Ctrl+Tab hides it
    if (gameMinimapShown()) {
        const bool again = s.hidOld && s.cellKnown;  // hidden on this map before: the game made it again
        if (again && s.remade++ < 3)
            LOG_INFO("minimap: the game made its own again on %s - a Ctrl+Tab it saw past ours, or the map anew",
                     map.c_str());
        if (hideGameMinimap()) {
            if (!s.hidOld) LOG_INFO("minimap: the game's own hidden on %s (its UI window 14 deleted)", map.c_str());
            s.hidOld = true;
        } else if (!s.hideFailed) {
            s.hideFailed = true;
            LOG_INFO("minimap: the game's own NOT hidden on %s yet - its windows busy (tried again each frame) or not "
                     "this uaRO.exe's window calls", map.c_str());
        }
    }
    int x = 0, y = 0, w = 0, h = 0;
    if (readPlayerCell(x, y, w, h)) {
        if (!s.cellKnown) LOG_INFO("minimap: the player on %s at %d, %d (the map %dx%d)", map.c_str(), x, y, w, h);
        if (!s.cellKnown || x != s.cell.x || y != s.cell.y || w != s.size.w || h != s.size.h)
            s.dirty = s.bigDirty = true;
        s.cell = minimap::Cell{x, y};
        s.size = minimap::MapSize{w, h};
        s.cellKnown = true;
    }
    float facing = 0, headE = 0, headN = 0;  // 3.0.11-beta.71: the arrow - the way the character faces
    if (readPlayerFacing(facing) && std::fabs(facing - s.facing) > 0.5f && minimap::facingOf(facing, headE, headN)) {
        if (s.facingLogged++ < 6)
            LOG_INFO("minimap: the character faces %.0f degrees (east %.2f, north %.2f) at %d, %d on %s - the arrow",
                     facing, headE, headN, s.cell.x, s.cell.y, s.map.c_str());
        s.facing = facing;
        s.headX = headE;
        s.headY = headN;
        s.dirty = s.bigDirty = true;
    }
    float east = 0, north = 0;  // 3.0.11-beta.31: the fan turns as the camera does (by a degree or more)
    if (readLook(east, north) && (std::fabs(east - s.lookX) > 0.017f || std::fabs(north - s.lookY) > 0.017f)) {
        if (s.lookX == 0 && s.lookY == 0)  // the first a map: to set the arrow against the screen
            LOG_INFO("minimap: the camera looks %.2f east, %.2f north on %s (the arrow that way)", east, north,
                     s.map.c_str());
        s.lookX = east;
        s.lookY = north;
        s.dirty = s.bigDirty = true;
    }
    // 3.0.11-beta.75: its size in «Розміщення інтерфейсу»; a new place lays it out again
    const float scale = uistyle::partScale(k) * hudedit::sizeOf(hudlayout::Part::Minimap);
    // 3.0.11-beta.79 (the owner: «окошко скачет сильно»): moved, it is drawn where it stands without painting again;
    // put back to its own place, laid out again
    if (const bool moved = hudedit::movedOf(hudlayout::Part::Minimap); moved != s.placedMoved) {
        s.placedMoved = moved;
        s.dirty = true;
    }
    if (float(k) != s.k || scale != s.scale || targetWidth != s.targetW || targetHeight != s.targetH) {
        s.k = float(k);
        s.scale = scale;
        s.targetW = targetWidth;
        s.targetH = targetHeight;
        s.dirty = s.bigDirty = true;
    }
    if (s.look == minimap::Look::Hidden && !s.big) {  // Ctrl+Tab hid it: neither drawn nor taking clicks
        if (s.shown) unblock();
        s.shown = false;
        return;
    }
    // 3.0.11-beta.6: the hints - made again for another map, another reference read, other switches
    const auto index = reference::index();
    const auto live = reference::live();  // 3.0.11-beta.12
    const ULONGLONG now = GetTickCount64();
    if (!s.hintsMade || s.hintsMap != s.map || index != s.index || show.npcs != s.show.npcs ||
        show.warps != s.show.warps || show.mobs != s.show.mobs || live != s.live ||
        (s.live && now - s.hintsAt >= 60000)) {
        s.live = live;
        s.hintsAt = now;
        if (index != s.index) {  // 3.0.11-beta.9: the way made again over the new pack
            s.follow.map.clear();
            s.cellsMap.clear();
        }
        s.index = index;
        s.show = show;
        buildHints(s);
        s.dirty = s.bigDirty = true;
    }
    if (!plotFrame(s)) followFrame(s);  // 3.0.11-beta.71: the way set on the map first
    {  // 3.0.11-beta.83: the party's members here, their faces as the marks' size
        const int side = std::max(8, int(std::lround(12 * s.scale)));
        std::vector<minimap::Mark> members;
        std::string key;
        for (const partywin::OnMap& m : partywin::onMap(side)) {
            minimap::Mark mark{minimap::Cell{m.x, m.y}, 0, minimap::Glyph::Member, m.name};
            mark.picture = m.face;
            mark.dead = m.dead;
            members.push_back(mark);
            key += m.name + ':' + std::to_string(m.x) + ',' + std::to_string(m.y) + (m.dead ? "d" : "") +
                   (m.face ? "f" : "") + ';';
        }
        if (key != s.membersKey) {
            s.membersKey = key;
            s.members = std::move(members);
            s.dirty = s.bigDirty = true;
        }
    }
    reachFrame(s, show.reach);  // 3.0.11-beta.65
    // the part and the mark under the game's cursor: a button lit, a mark naming itself
    int cx = 0, cy = 0, hover = -1;
    minimap::Part part = minimap::Part::None;
    minimap::BigPart bigPart = minimap::BigPart::None;
    if (readGameCursor(cx, cy)) {
        const float tx = float(cx) * float(k), ty = float(cy) * float(k);
        const minimap::Scene sc = sceneOf(s);
        if (s.big) {
            const float bx = tx - float(s.bigLayout.x), by = ty - float(s.bigLayout.y);
            bigPart = minimap::bigPartAt(s.bigLayout, bx, by);
            if (bigPart == minimap::BigPart::Map) hover = minimap::bigMarkAt(s.bigLayout, sc, bx, by);
        }
        if (bigPart == minimap::BigPart::None) {
            const float px = tx - float(s.layout.x), py = ty - float(s.layout.y);
            part = minimap::partAt(s.layout, px, py);
            if (part == minimap::Part::Map) hover = minimap::markAt(s.layout, sc, px, py);
        }
    }
    if (s.big && (bigPart != s.bigPart || hover != s.hover)) s.bigDirty = true;
    s.bigPart = bigPart;
    if (hover != s.hover || part != s.part) {
        s.hover = hover;
        s.part = part;
        s.dirty = true;
    }
    if (s.style != uistyle::generation()) {  // 3.0.11-beta.47: its bar's ground another opacity
        s.style = uistyle::generation();
        s.dirty = true;
    }
    // 3.0.11-beta.57: at most each kPaintEveryMs (the first at once)
    if (s.dirty && (s.picture.width <= 0 || now - s.paintedAt >= kPaintEveryMs)) render(s);
    else  // 3.0.11-beta.75: known to «Розміщення інтерфейсу» each frame (painted or not)
        hudedit::place(hudlayout::Part::Minimap, s.k, s.targetW, s.targetH, s.layout.w, s.layout.h, s.layout.x,
                       s.layout.y);
    if (s.big && s.bigDirty && (s.bigPicture.width <= 0 || now - s.bigPaintedAt >= kPaintEveryMs)) renderBig(s);
    s.shown = true;
    block(s, game);
}

bool key(unsigned vk) {
    State& s = state();
    if (!s.on) return false;
    if (vk == VK_ESCAPE && s.big) {  // 3.0.11-beta.31: Esc closes the big map first
        setBig(s, false);
        return true;
    }
    if (vk != VK_TAB || !(GetKeyState(VK_CONTROL) & 0x8000)) return false;
    s.look = minimap::next(s.look);
    s.dirty = true;
    static const char* const kNames[] = {"opaque", "see-through", "hidden"};
    LOG_INFO("minimap: Ctrl+Tab - the new minimap %s", kNames[int(s.look)]);
    return true;
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the answer card
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    size_t mine = 0;
    if (s.look != minimap::Look::Hidden && s.picture.width > 0 && s.picture.height > 0) {
        drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
        ++mine;
    }
    if (s.big && s.bigPicture.width > 0) {  // 3.0.11-beta.31: the big map over the rest of the game's picture
        drawPart(record, renderer, s.bigPicture, s.bigTexture, s.bigTexW, s.bigTexH, s.bigUpload, s.bigLayout.x,
                    s.bigLayout.y, s.k);
        ++mine;
    }
    if (cursor >= 0 && mine > 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    if (m == WM_MOUSEWHEEL && !ScreenToClient(game, &p)) return false;  // the wheel's point is the screen's
    float tx = 0, ty = 0;
    if (!toTarget(s, game, p.x, p.y, tx, ty)) return false;
    const minimap::Scene sc = sceneOf(s);
    if (s.big) {  // 3.0.11-beta.31: the big map first - it lies over the rest
        const float bx = tx - float(s.bigLayout.x), by = ty - float(s.bigLayout.y);
        const minimap::BigPart bp = minimap::bigPartAt(s.bigLayout, bx, by);
        if (bp != minimap::BigPart::None) {
            if (m == WM_LBUTTONDOWN && bp == minimap::BigPart::Close) setBig(s, false);
            const bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
            minimap::Cell at;
            if (m == WM_LBUTTONDOWN && bp == minimap::BigPart::Map && !ctrl &&
                minimap::bigCellAt(s.bigLayout, sc, bx, by, at)) {  // 3.0.11-beta.71: the way there (Shift: on)
                setPlot(s, at, shift);
                plotFrame(s);
            }
            if (m == WM_RBUTTONDOWN && bp == minimap::BigPart::Map) clearPlot(s);
            if (m == WM_LBUTTONDOWN && bp == minimap::BigPart::Map && ctrl) {  // Ctrl: a warp's map's card
                const int mark = minimap::bigMarkAt(s.bigLayout, sc, bx, by);
                if (mark >= 0 && size_t(mark) < s.marks.size() && s.index &&
                    s.marks[size_t(mark)].glyph == minimap::Glyph::Warp) {
                    const auto it = s.index->maps.find(s.map);
                    if (it != s.index->maps.end())
                        for (const refindex::Warp& w : it->second.warps)
                            if (std::abs(w.x - s.marks[size_t(mark)].cell.x) <= 2 &&
                                std::abs(w.y - s.marks[size_t(mark)].cell.y) <= 2) {
                                referencewin::openMap(w.dest);
                                break;
                            }
                }
            }
            result = 0;
            return true;
        }
    }
    if (s.look == minimap::Look::Hidden) return false;
    const float px = tx - float(s.layout.x), py = ty - float(s.layout.y);
    const minimap::Part part = minimap::partAt(s.layout, px, py);
    if (part == minimap::Part::None) return false;
    if (m == WM_MOUSEWHEEL) {  // 3.0.11-beta.31: the wheel over it zooms
        if (part == minimap::Part::Map) setZoom(s, s.zoom + (GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1));
        result = 0;
        return true;
    }
    if (m == WM_LBUTTONDOWN && s.clicks++ == 0)  // the check of the prototype: the character did not walk
        LOG_INFO("minimap: a click on the new minimap kept from the game (the player at %d, %d on %s)", s.cell.x,
                 s.cell.y, s.map.c_str());
    if (m == WM_LBUTTONDOWN) {
        switch (part) {
            case minimap::Part::ZoomIn: setZoom(s, s.zoom - 1); break;  // fewer cells: nearer
            case minimap::Part::ZoomOut: setZoom(s, s.zoom + 1); break;
            case minimap::Part::Expand: setBig(s, !s.big); break;
            case minimap::Part::Fold:
                s.folded = !s.folded;
                s.dirty = true;
                keepState(s);
                break;
            case minimap::Part::Stop:  // 3.0.11-beta.71: the way set on the map first
                if (!s.plot.empty() || s.plotThereAt) clearPlot(s);
                else unfollow();
                break;
            case minimap::Part::Caption: referencewin::openMap(s.map); break;  // 3.0.11-beta.7: its card
            case minimap::Part::Route:
                if (s.plot.empty() && !s.plotThereAt) referencewin::openMap(s.follow.target);
                break;
            case minimap::Part::List: {  // 3.0.11-beta.9: a monster's card; «і ще N» - the map's
                const int i = minimap::lineAt(s.layout, py);
                if (i >= 0 && size_t(i) < s.lines.size()) {
                    if (s.lines[size_t(i)].mob > 0) referencewin::openMob(s.lines[size_t(i)].mob);
                    else referencewin::openMap(s.map);
                }
                break;
            }
            case minimap::Part::Map: {  // 3.0.11-beta.71 (the owner: «хочется на карте прокладывать путь»)
                minimap::Cell at;
                if (minimap::cellAt(s.layout, sc, px, py, at)) {
                    setPlot(s, at, GetKeyState(VK_SHIFT) < 0);
                    plotFrame(s);
                }
                break;
            }
            case minimap::Part::None: break;
        }
    }
    if (m == WM_RBUTTONDOWN && part == minimap::Part::Map) clearPlot(s);  // 3.0.11-beta.71: the way set taken away
    result = 0;
    return true;
}

bool showBig(bool open) {
    State& s = state();
    if (!s.on) return false;
    setBig(s, open);
    return true;
}

bool bigShown() { return state().on && state().big; }

bool canFollow() { return state().on; }

bool plotTo(int x, int y) {
    State& s = state();
    if (!s.on || s.map.empty() || x < 0 || y < 0 || (s.size.w > 0 && (x >= s.size.w || y >= s.size.h))) return false;
    setPlot(s, minimap::Cell{x, y}, false);
    s.dirty = s.bigDirty = true;
    return true;
}

bool plotted(int& x, int& y) {
    const State& s = state();
    if (s.plot.empty() || s.plotMap != s.map) return false;
    x = s.plot.back().x;
    y = s.plot.back().y;
    return true;
}

std::vector<std::pair<std::string, std::string>> plotLines() {
    std::vector<std::pair<std::string, std::string>> out;
    for (const minimap::Line& l : state().plotLines) out.emplace_back(l.left, l.right);
    return out;
}

void clearPlot() { clearPlot(state()); }

std::vector<std::pair<std::string, std::string>> routeLines() {
    std::vector<std::pair<std::string, std::string>> out;
    for (const minimap::Line& l : state().routeLines) out.emplace_back(l.left, l.right);
    return out;
}

bool panelBox(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown || s.look == minimap::Look::Hidden || s.layout.w <= 0) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

void follow(const std::string& target, bool walk) {
    State& s = state();
    s.follow = routefollow::Follow{};
    s.follow.active = !target.empty();
    s.follow.target = target;
    s.follow.walk = walk;
    s.arrivedAt = 0;
    s.pathFrom = minimap::Cell{-1, -1};
    s.kept = walkpath::Kept{};  // 3.0.11-beta.34: chosen again: made again
    LOG_INFO("route: the way to %s %s followed on the minimap", target.c_str(), walk ? "on foot" : "by Kafra");
    followFrame(s);
    s.dirty = s.bigDirty = true;
}

void unfollow() {
    State& s = state();
    if (!s.follow.active) return;
    LOG_INFO("route: the way to %s no more followed", s.follow.target.c_str());
    s.follow = routefollow::Follow{};
    s.routeLines.clear();
    s.path.clear();
    s.kept = walkpath::Kept{};
    s.dirty = s.bigDirty = true;
    toGround(s);
}

bool following(std::string& target, bool& walk) {
    const State& s = state();
    if (!s.follow.active) return false;
    target = s.follow.target;
    walk = s.follow.walk;
    return true;
}

}  // namespace uagfx::win::minimapproto
