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

#include "core/game_packets.hpp"
#include "core/layout.hpp"
#include "core/minimap_proto.hpp"
#include "core/mob_search.hpp"
#include "core/pack_source.hpp"
#include "core/reference_index.hpp"
#include "core/reference_live.hpp"
#include "core/reference_pages.hpp"
#include "core/route_follow.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"
#include "core/walk_grid.hpp"
#include "core/walk_path.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/guild_card.hpp"
#include "win/game_files.hpp"
#include "win/ground_way.hpp"
#include "win/head_picture.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/map_hd.hpp"
#include "win/net_watch.hpp"
#include "win/party_panel.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/reference_window.hpp"
#include "win/window_input.hpp"
#include "win/window_stack.hpp"
#include "win/ui_cost.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"
#include "win/words.hpp"

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
    // 3.0.18-beta.133 (the pad analysis, item 2: the big map on the controller): its cross - the cell, the moves'
    // fractions, whether the pad drives it now
    bool padCross = false;
    minimap::Cell padCell;
    float padFx = 0, padFy = 0;
    minimap::MapCache mapCache;  // 3.0.13-beta.2
    int cacheTold = 0;
    // 3.0.11-beta.119 (R-5Z6J2S): a layer switched on the map - wanted so until the settings read again say it
    // (uaro-graphics.ini written at once; the file's next reading a moment later): -1 none, else 0 / 1
    int layerWant[minimap::kLayers] = {-1, -1, -1, -1};
    ULONGLONG layerWantAt[minimap::kLayers] = {};
    int hoverLayer = -1;
    std::string net;  // 3.0.11-beta.122: the round trip's badge painted
    uint32_t netRgb = 0;
    std::vector<int> netGraph;  // 3.0.11-beta.127: its plate's graph painted (the last minute, ms; -1 a stall)
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
    // 3.0.11-beta.120: «Схема» - the map drawn from its walkable cells (made again for another map or part), the style
    // switched on the map wanted till the settings say it
    minimap::MapPicture scheme;
    std::string schemeKey;
    int styleWant = -1;
    ULONGLONG styleWantAt = 0;
    bool hdOn = true, hdAsked = false;
    minimap::MapSize size;
    minimap::Cell cell;
    bool cellKnown = false;
    int clicks = 0;  // kept from the game on this map
    int remade = 0;  // 3.0.11-beta.4: the game made its own again on this map after we hid it (a Ctrl+Tab it saw)
    minimap::Layout layout;
    panel::Picture picture;
    // 3.0.18-beta.26: uaUI's minimap - the map's square apart, the frame's words and the graph's bars
    panel::Picture square;
    int squareGen = 0, uiGen = 0;
    bool uiShown = false;
    std::vector<minimap::Words> uiWords;
    std::vector<minimap::NetBar> uiBars;
    bool paintedByUi = false;
    // 3.0.18-beta.42: uaUI's big map - its square apart, its words
    uisheet::Sheet bigSheet;
    panel::Picture bigSquare;
    int bigGen = 0;
    bool bigShown = false, bigByUi = false;
    std::vector<minimap::Words> bigUiWords;
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
    // 3.0.11-beta.196 (the tester: «значок кафри ... сприймається як значок хрестика»): the Kafras' marks with a
    // Kafra's face (4_F_KAFRA1, the classic one: one sign for them all), made once a size
    panel::Picture kafraFace;
    int kafraSide = 0;
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
    bool bigTop = false;  // 3.0.11-beta.260: over the windows
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
    // 3.0.11-beta.201 (the tester: «ще не видно согільдійців на мінімапі», «команда типу пошук мобів на мінімапі»): the
    // server's marks (0x0144: an NPC's viewpoint, @showmobs' monsters) by their NPC and number, the guild's mates
    // (0x01eb) by their account with when last told; both forgotten with the map
    struct Spot {
        int x = 0, y = 0;
        uint32_t colour = 0xFFFFFF;
    };
    struct Mate {
        int x = 0, y = 0;
        ULONGLONG at = 0;
    };
    std::map<std::pair<uint32_t, int>, Spot> spots;
    std::map<uint32_t, Mate> mates;
    mobsearch::Reader found;  // 3.0.11-beta.213: @mobsearch's answer
    std::string othersKey;
    std::vector<minimap::Mark> others;
};

constexpr ULONGLONG kPaintEveryMs = 33;
constexpr uint32_t kOwnAccount = 0x771F7C;  // 3.0.11-beta.201: the character's account (not its own mate)

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

// 3.0.11-beta.196: the Kafras' face at the marks' size (as the party's faces); null: the sprite not found - the «K»
const panel::Picture* kafraFace(State& s) {
    const int side = std::max(8, int(std::lround(12 * s.scale)));
    if (side != s.kafraSide) {
        s.kafraSide = side;
        std::string from;
        const bool made = headpicture::npcFace("4_F_KAFRA1", side, s.kafraFace, &from);
        LOG_INFO("minimap: the Kafras' face - %s", made ? from.c_str() : (from + ", their «K»").c_str());
    }
    return s.kafraFace.width > 0 ? &s.kafraFace : nullptr;
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
    if (h && (s.show.npcs || s.show.kafra)) {  // 3.0.11-beta.119: the Kafras a layer of their own
        const panel::Picture* face = kafraFace(s);
        for (const refindex::Npc& n : h->npcs) {
            const bool kafra = n.kind == refindex::NpcKind::Kafra;
            if (kafra ? !s.show.kafra : !s.show.npcs) continue;
            s.marks.push_back(minimap::Mark{minimap::Cell{n.x, n.y}, refindex::colorOf(n.kind),
                                            kafra ? minimap::Glyph::Kafra : minimap::Glyph::Dot, n.name});
            if (kafra) s.marks.back().picture = face;
            else s.marks.back().letter = refindex::letterOf(n.kind);  // 3.0.18-beta.240: «$», «!» on the big map
        }
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
            s.marks.back().exit = w.dest.find("_in") == std::string::npos;  // 3.0.18-beta.240: its words always
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
        if (boss && m.respawnMs >= kBossRespawnMs) right = refpages::respawn(m.respawnMs, language());
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
                right = left > 0 ? refpages::respawn(int(std::max<long long>(left, 60) * 1000), language())
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
    s.routeLines.push_back(minimap::Line{refpages::stepNote(*step, language()), ""});
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
void setPlot(State& s, minimap::Cell at, bool add) {
    // 3.0.11-beta.201 (the tester: «ставиш точку ... в непрохідне місце - малює лінію пряму», «не давати можливість
    // поставити точку поза мапою, або будувати шлях до найближчої прохідної клітинки»): a cell off the walked ones -
    // the nearest walked one; off the map or none within 40 cells: no point
    loadCells(s);
    if (s.cells.w > 0) {
        const walkpath::Cell walked = walkpath::nearestWalked(s.cells, walkpath::Cell{at.x, at.y}, 40);
        if (walked.first < 0) {
            LOG_INFO("route: %d, %d on %s is off its walked cells, none near - no point set", at.x, at.y,
                     s.map.c_str());
            return;
        }
        if (walked.first != at.x || walked.second != at.y)
            LOG_INFO("route: %d, %d on %s is off its walked cells - the point at the nearest walked one, %d, %d", at.x,
                     at.y, s.map.c_str(), walked.first, walked.second);
        at = minimap::Cell{walked.first, walked.second};
    }
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
// of the player's part of the map (walkgrid - the server's own .gat, parts joined across no wall) left lit, the other
// parts' walked-on cells dimmed (3.0.11-beta.126: the walls and the ground as drawn - R-4ZHJ3R's c_tower2 one grey);
// made again only when the player is in another part (a teleport, a warp, a map) - a step changes nothing.
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
    s.reach = walkgrid::reachMask(*grid, part);  // 3.0.11-beta.126: walls apart from the other parts
    s.reachMap = s.map;
    s.reachPart = part;
    s.dirty = s.bigDirty = true;
    if (s.reachLogged++ < 20) {
        const size_t lit = size_t(std::count(s.reach.begin(), s.reach.end(), uint8_t(1)));
        const size_t dim = size_t(std::count(s.reach.begin(), s.reach.end(), walkgrid::kElsewhere));
        LOG_INFO("minimap: where one can walk on %s from %d, %d - part %d of %d, %zu of %dx%d cells; %zu of the other "
                 "parts dimmed, the walls as drawn (%llu ms)",
                 s.map.c_str(), s.cell.x, s.cell.y, int(part), grid->partCount, lit, grid->w, grid->h, dim,
                 static_cast<unsigned long long>(GetTickCount64() - t0));
    }
}

// 3.0.11-beta.72: the map's picture shown - four times bigger once found ([interface] minimap_hd), else the game's;
// 3.0.11-beta.120: «Схема» when chosen and the map's cells are known
const minimap::MapPicture* shownPicture(const State& s) {
    if (s.show.scheme && !s.scheme.levels.empty()) return &s.scheme;
    const minimap::MapPicture* p = s.hdOn && s.hd ? s.hd.get() : &s.prepared;
    return p->levels.empty() ? nullptr : p;
}

// 3.0.11-beta.120: «Схема» made when chosen - the map's walkable cells, the part the player stands in lighter (4
// pixels a cell); nothing for a map the walkable file lacks (the game's picture then)
void schemeFrame(State& s) {
    if (!s.show.scheme || !s.index || !s.index->walk) return;
    const auto it = s.index->walk->find(s.map);
    if (it == s.index->walk->end() || it->second.w != s.size.w || it->second.h != s.size.h) {
        if (!s.scheme.levels.empty()) {
            s.scheme = minimap::MapPicture{};
            s.schemeKey.clear();
            s.dirty = s.bigDirty = true;
        }
        return;
    }
    const walkgrid::Grid& grid = it->second;
    const uint8_t part = s.cellKnown ? walkgrid::partOf(grid, s.cell.x, s.cell.y) : walkgrid::kWall;
    const std::string key = s.map + ':' + std::to_string(part);
    if (key == s.schemeKey || (part == walkgrid::kWall && !s.schemeKey.empty() &&
                               s.schemeKey.compare(0, s.map.size() + 1, s.map + ':') == 0))
        return;  // the same, or on a wall a moment: the part kept
    const ULONGLONG t0 = GetTickCount64();
    const std::vector<uint8_t> mask = part == walkgrid::kWall ? std::vector<uint8_t>{} : walkgrid::partMask(grid, part);
    int w = 0, h = 0;
    const std::vector<uint32_t> pixels = minimap::schemePixels(grid, mask.empty() ? nullptr : &mask, 4, w, h);
    s.scheme = minimap::prepare(pixels, w, h);
    s.schemeKey = key;
    s.dirty = s.bigDirty = true;
    LOG_INFO("minimap: «Схема» of %s (part %d, %dx%d cells) made in %llu ms", s.map.c_str(), int(part), grid.w, grid.h,
             static_cast<unsigned long long>(GetTickCount64() - t0));
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

minimap::Layers layersOf(const Show& show) {  // 3.0.11-beta.119
    minimap::Layers l;
    l.on[int(minimap::Layer::Warps)] = show.warps;
    l.on[int(minimap::Layer::Npcs)] = show.npcs;
    l.on[int(minimap::Layer::Kafra)] = show.kafra;
    l.on[int(minimap::Layer::Party)] = show.party;
    return l;
}

const char* const kLayerKeys[minimap::kLayers] = {"minimap_warps", "minimap_npcs", "minimap_kafra", "minimap_party"};

// 3.0.11-beta.119: what the settings say, a layer just switched on the map as wanted till they say it too (5 s at most)
Show withWanted(State& s, Show show, ULONGLONG now) {
    bool* flags[minimap::kLayers] = {&show.warps, &show.npcs, &show.kafra, &show.party};
    for (int i = 0; i < minimap::kLayers; ++i) {
        if (s.layerWant[i] < 0) continue;
        if (*flags[i] == (s.layerWant[i] == 1) || now - s.layerWantAt[i] > 5000) {
            s.layerWant[i] = -1;  // said by the settings now (or not written: theirs again)
            continue;
        }
        *flags[i] = s.layerWant[i] == 1;
    }
    if (s.styleWant >= 0) {  // 3.0.11-beta.120: «Схема» likewise
        if (show.scheme == (s.styleWant == 1) || now - s.styleWantAt > 5000) s.styleWant = -1;
        else show.scheme = s.styleWant == 1;
    }
    return show;
}

// 3.0.11-beta.119: a layer's button or chip clicked - on or off at once, uaro-graphics.ini told (the app's box too)
void switchLayer(State& s, int layer) {
    if (layer < 0 || layer >= minimap::kLayers) return;
    const bool on = !layersOf(s.show).on[layer];
    s.layerWant[layer] = on ? 1 : 0;
    s.layerWantAt[layer] = GetTickCount64();
    s.hintsMade = false;  // the marks made again with it
    s.dirty = s.bigDirty = true;
    const bool written = editIniFile(ourFile(layout::File::Ini), {{"interface", kLayerKeys[layer], on ? "1" : "0"}});
    LOG_INFO("minimap: the layer %s %s on the map%s", kLayerKeys[layer], on ? "on" : "off",
             written ? " ([interface] written)" : " - uaro-graphics.ini could not be written");
}

// 3.0.11-beta.120: the picture's switch clicked - «Схема» or the game's picture at once, uaro-graphics.ini told
void switchStyle(State& s) {
    const bool scheme = !s.show.scheme;
    s.styleWant = scheme ? 1 : 0;
    s.styleWantAt = GetTickCount64();
    s.show.scheme = scheme;
    s.dirty = s.bigDirty = true;
    const bool written =
        editIniFile(ourFile(layout::File::Ini), {{"interface", "minimap_style", scheme ? "scheme" : "picture"}});
    LOG_INFO("minimap: «%s» on the map%s", scheme ? "Схема" : "the game's picture",
             written ? " ([interface] minimap_style written)" : " - uaro-graphics.ini could not be written");
}

minimap::Scene sceneOf(const State& s) {
    minimap::Scene sc;
    sc.picture = shownPicture(s);  // 3.0.11-beta.72 (no argb: never prepared again at each painting)
    sc.map = s.size;
    sc.player = s.cell;
    sc.lookX = s.lookX;
    sc.lookY = s.lookY;
    sc.viewFan = s.show.viewFan;  // 3.0.11-beta.187: [interface] minimap_view
    sc.classicMarks = s.show.classic;  // 3.0.11-beta.222: [interface] minimap_classic
    sc.cells = minimap::cellsAt(s.zoom, s.size);
    sc.marks = s.marks;
    if (s.show.party)  // 3.0.11-beta.119: the party a layer
        sc.marks.insert(sc.marks.end(), s.members.begin(), s.members.end());  // 3.0.11-beta.83: after the hints
    // 3.0.11-beta.201: the guild's mates (the party's layer) and the server's marks (always)
    for (const minimap::Mark& m : s.others)
        if (m.glyph == minimap::Glyph::Spot || s.show.party) sc.marks.push_back(m);
    sc.kafraFace = s.kafraFace.width > 0 ? &s.kafraFace : nullptr;  // 3.0.11-beta.201: the layer's button
    sc.layers = layersOf(s.show);
    sc.hoverLayer = s.hoverLayer;
    sc.scheme = s.show.scheme && !s.scheme.levels.empty();  // 3.0.11-beta.120
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
    if (!s.net.empty() && !s.netGraph.empty()) sc.netGraph = &s.netGraph;  // 3.0.11-beta.127
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

std::string layerName(int i) {  // 3.0.11-beta.119
    using settings::Text;
    const Text names[minimap::kLayers] = {Text::MinimapLayerWarps, Text::MinimapLayerNpcs, Text::MinimapLayerKafra,
                                          Text::MinimapLayerParty};
    return settings::tr(language(), names[i]);
}

// 3.0.11-beta.213: @mobsearch's answer for the map shown
bool foundHere(const State& s) {
    return s.found.active() && !s.found.result().points.empty() && mobsearch::sameMap(s.found.result().map, s.map);
}

void render(State& s) {
    minimap::Texts t;
    t.name = shortTitle(s, s.map);
    if (s.cellKnown) t.cell = std::to_string(s.cell.x) + ", " + std::to_string(s.cell.y);
    t.folded = s.folded;
    for (int i = 0; i < minimap::kLayers; ++i) t.layers[i] = layerName(i);  // 3.0.11-beta.119
    t.style = settings::tr(language(), settings::Text::MinimapLayerScheme);  // 3.0.11-beta.120
    t.net = s.net;  // 3.0.11-beta.122
    t.netRgb = s.netRgb;
    if (foundHere(s))  // 3.0.11-beta.213: «Poring · 12»
        t.found = s.found.result().what + " \xC2\xB7 " + std::to_string(s.found.result().points.size());
    t.route = !s.plotLines.empty() ? s.plotLines : s.routeLines;  // 3.0.11-beta.71: the way set on the map first
    for (const Line& l : s.lines)
        t.list.push_back(minimap::Line{l.left, l.right, l.boss ? kBossText : 0u,
                                       l.timer ? minimap::kRouteRgb : 0u, l.boss});
    // 3.0.18-beta.51 (the owner: «увеличил карту, а там иконки маленькие»): an enlarged minimap's buttons and marks
    // bigger still - by its size's root
    const float grow = std::sqrt(std::max(1.0f, hudedit::sizeOf(hudlayout::Part::Minimap)));
    s.layout = minimap::layout(s.targetW, s.targetH, s.scale, !t.route.empty(), s.folded ? 0 : int(t.list.size()),
                               !t.net.empty(), !t.found.empty(), grow);  // .127: the round trip's plate; .213 search
    hudedit::place(hudlayout::Part::Minimap, s.k, s.targetW, s.targetH, s.layout.w, s.layout.h, s.layout.x,
                   s.layout.y);  // 3.0.11-beta.75: where the player put it
    const uicost::Paint cost("minimap", s.layout.w, s.layout.h);  // 3.0.11-beta.55 (beta.57: its size known)
    s.paintedAt = GetTickCount64();
    minimap::Scene sc = sceneOf(s);
    sc.cache = &s.mapCache;  // 3.0.13-beta.2: the map's pixels made once round the view
    const int madeBefore = s.mapCache.made;
    minimap::paintMapSquare(s.square, s.layout, sc);
    if (s.mapCache.made != madeBefore && s.cacheTold < 5) {
        ++s.cacheTold;
        LOG_INFO("minimap: the map's pixels made round the view (%dx%d) - the steps copy them out", s.mapCache.w,
                 s.mapCache.h);
    }
    drawWords(s.square, gdiWords(minimap::mapWords(s.layout, sc)));
    ++s.squareGen;
    s.uiWords = minimap::frameWords(s.layout, sc, t);
    s.uiBars = minimap::netBars(s.layout, sc);
    ++s.uiGen;
    s.dirty = false;
}

void renderBig(State& s) {
    const minimap::Scene sc = sceneOf(s);
    std::string names[minimap::kLayers];  // 3.0.11-beta.119: the layers' chips under the map, as wide as their words
    float widths[minimap::kLayers] = {};
    for (int i = 0; i < minimap::kLayers; ++i) {
        names[i] = layerName(i);
        const float s28 = std::round(28 * s.scale);
        widths[i] = std::round(float(wordsWidth(utf8Wide(names[i]), 12 * s.scale, true)) + s28 + 14 * s.scale);
    }
    const std::string styleName = settings::tr(language(), settings::Text::MinimapLayerScheme);  // .120
    const float styleWidth =
        std::round(float(wordsWidth(utf8Wide(styleName), 12 * s.scale, true)) + std::round(28 * s.scale) +
                   14 * s.scale);
    s.bigLayout = minimap::bigLayout(s.targetW, s.targetH, s.scale, s.size, widths, styleWidth);
    const uicost::Paint cost("big map", s.bigLayout.w, s.bigLayout.h);  // 3.0.11-beta.55 (beta.57: its size known)
    s.bigPaintedAt = GetTickCount64();
    const int picture = sc.picture ? sc.picture->levels.front().w : 0;  // 3.0.11-beta.72: the game's or the bigger
    const std::string key = s.map + "|" + std::to_string(s.bigLayout.w) + "x" + std::to_string(s.bigLayout.h) + "|" +
                            std::to_string(picture) + "|" + std::to_string(s.reach.empty() ? -1 : int(s.reachPart)) +
                            "|" + (sc.scheme ? s.schemeKey : std::string());  // 3.0.11-beta.120
    if (key != s.bigMapOf) {  // the map's picture at its size: made once
        s.bigMapOf = key;
        minimap::paintBigMap(s.bigMap, s.bigLayout, sc);
    }
    const std::string cell = s.cellKnown ? std::to_string(s.cell.x) + ", " + std::to_string(s.cell.y) : "";
    minimap::paintBigSquare(s.bigSquare, s.bigMap, s.bigLayout, sc);
    s.bigUiWords = minimap::bigWords(s.bigLayout, sc, shortTitle(s, s.map) + " \xC2\xB7 " + s.map, cell, names,
                                     styleName);
    ++s.bigGen;
    s.bigDirty = false;
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
    s.bigTop = open;  // 3.0.11-beta.260: opened - over the windows
    s.bigDirty = true;
    s.bigPaintedAt = 0;  // 3.0.11-beta.57: opened - painted at once
    s.bigPart = minimap::BigPart::None;
    LOG_INFO("minimap: the big map %s", open ? "opened" : "closed");
}

// 3.0.11-beta.187: `by` steps nearer (-1) or farther (+1), past those that show the whole map or more
void setZoom(State& s, int by) {
    const int step = minimap::zoomStep(s.zoom, by, s.size);
    if (step == s.zoom) return;
    s.zoom = step;
    s.dirty = true;
    keepState(s);
}

}  // namespace

static void frameInner(HWND game, const Show& show, double k, int targetWidth, int targetHeight) {
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
        s.spots.clear();  // 3.0.11-beta.201: the server's marks and the guild's mates were the map's
        s.mates.clear();
        if (s.found.active() && !mobsearch::sameMap(s.found.result().map, map)) s.found.clear();  // .213
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
    const Show want = withWanted(s, show, now);  // 3.0.11-beta.119: a layer switched on the map
    if (!s.hintsMade || s.hintsMap != s.map || index != s.index || want.npcs != s.show.npcs ||
        want.warps != s.show.warps || want.mobs != s.show.mobs || want.kafra != s.show.kafra ||
        want.party != s.show.party || live != s.live || (s.live && now - s.hintsAt >= 60000)) {
        s.live = live;
        s.hintsAt = now;
        if (index != s.index) {  // 3.0.11-beta.9: the way made again over the new pack
            s.follow.map.clear();
            s.cellsMap.clear();
        }
        s.index = index;
        s.show = want;
        buildHints(s);
        s.dirty = s.bigDirty = true;
    }
    if (want.classic != s.show.classic) {  // 3.0.11-beta.222: [interface] minimap_classic
        s.show.classic = want.classic;
        s.dirty = s.bigDirty = true;
    }
    if (want.viewFan != s.show.viewFan) {  // 3.0.11-beta.187: [interface] minimap_view
        s.show.viewFan = want.viewFan;
        s.dirty = s.bigDirty = true;
    }
    if (want.scheme != s.show.scheme) {  // 3.0.11-beta.120: «Схема» or the game's picture
        s.show.scheme = want.scheme;
        s.dirty = s.bigDirty = true;
    }
    schemeFrame(s);
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
    {  // 3.0.11-beta.201: the guild's mates told in the last 20 s, the server's marks
        const ULONGLONG now = GetTickCount64();
        std::vector<minimap::Mark> others;
        std::string key;
        for (auto it = s.mates.begin(); it != s.mates.end();) {
            if (now - it->second.at > 20000) {
                it = s.mates.erase(it);
                continue;
            }
            if (partywin::inParty(it->first)) {  // 3.0.16-beta.38 (R-GF8Z5A): the party's face is its mark
                ++it;
                continue;
            }
            const std::string name = guildcardwin::mateName(it->first);  // 3.0.11-beta.219: its name
            others.push_back(minimap::Mark{minimap::Cell{it->second.x, it->second.y}, 0, minimap::Glyph::Guild,
                                           name.empty() ? say(settings::Text::MinimapGuildMate) : name});
            key += 'g' + std::to_string(it->second.x) + ',' + std::to_string(it->second.y) + ';';
            ++it;
        }
        for (const auto& [id, spot] : s.spots) {
            others.push_back(minimap::Mark{minimap::Cell{spot.x, spot.y}, spot.colour, minimap::Glyph::Spot, ""});
            key += 's' + std::to_string(spot.x) + ',' + std::to_string(spot.y) + ',' + std::to_string(spot.colour) +
                   ';';
        }
        if (foundHere(s)) {  // 3.0.11-beta.213: @mobsearch's monsters
            for (const mobsearch::Point& p : s.found.result().points) {
                others.push_back(minimap::Mark{minimap::Cell{p.x, p.y}, minimap::kFoundRgb, minimap::Glyph::Spot,
                                               p.name});
                key += 'f' + std::to_string(p.x) + ',' + std::to_string(p.y) + ';';
            }
            key += s.found.result().what;
        }
        if (key != s.othersKey) {
            s.othersKey = key;
            s.others = std::move(others);
            s.dirty = s.bigDirty = true;
        }
        if (s.kafraFace.width <= 0 && s.kafraSide == 0) kafraFace(s);  // the layer's button's face before any Kafra
    }
    reachFrame(s, show.reach);  // 3.0.11-beta.65
    // the part and the mark under the game's cursor: a button lit, a mark naming itself
    int cx = 0, cy = 0, hover = -1;
    minimap::Part part = minimap::Part::None;
    minimap::BigPart bigPart = minimap::BigPart::None;
    int hoverLayer = -1;  // 3.0.11-beta.119
    if (readHoverCursor(cx, cy)) {
        const float tx = float(cx) * float(k), ty = float(cy) * float(k);
        const minimap::Scene sc = sceneOf(s);
        if (s.big) {
            const float bx = tx - float(s.bigLayout.x), by = ty - float(s.bigLayout.y);
            bigPart = minimap::bigPartAt(s.bigLayout, bx, by);
            if (bigPart == minimap::BigPart::Map) hover = minimap::bigMarkAt(s.bigLayout, sc, bx, by);
            if (bigPart == minimap::BigPart::Layer) hoverLayer = minimap::bigLayerAt(s.bigLayout, bx, by);
        }
        if (bigPart == minimap::BigPart::None) {
            const float px = tx - float(s.layout.x), py = ty - float(s.layout.y);
            part = minimap::partAt(s.layout, px, py);
            if (part == minimap::Part::Map) hover = minimap::markAt(s.layout, sc, px, py);
            if (part == minimap::Part::Layer) hoverLayer = minimap::layerAt(s.layout, px, py);
        }
    }
    if (hoverLayer != s.hoverLayer) {
        s.hoverLayer = hoverLayer;
        s.dirty = true;
        if (s.big) s.bigDirty = true;
    }
    if (s.big && s.padCross) {  // 3.0.18-beta.133: the pad's cross - the mark within 3 cells of it named
        int best = -1, bestD = 4;
        for (size_t i = 0; i < s.marks.size(); ++i) {
            const int d =
                std::max(std::abs(s.marks[i].cell.x - s.padCell.x), std::abs(s.marks[i].cell.y - s.padCell.y));
            if (d < bestD) bestD = d, best = int(i);
        }
        hover = best;
        bigPart = minimap::BigPart::Map;
    }
    if (s.big && (bigPart != s.bigPart || hover != s.hover)) s.bigDirty = true;
    s.bigPart = bigPart;
    if (hover != s.hover || part != s.part) {
        s.hover = hover;
        s.part = part;
        s.dirty = true;
    }
    {  // 3.0.11-beta.122: the round trip's badge - green, amber, red, a stall
        std::string net;
        uint32_t rgb = 0;
        const netwatch::Level level = show.netBadge ? netwatchwin::level() : netwatch::Level::Unknown;
        if (level == netwatch::Level::Stall) {
            net = settings::tr(language(), settings::Text::NetStall);
            rgb = 0xFF5A4E;
        } else if (level != netwatch::Level::Unknown) {
            std::string ms = settings::tr(language(), settings::Text::NetMs);
            const int rtt = (netwatchwin::rttMs() + 2) / 5 * 5;  // in fives: not painted again each second
            if (const size_t at = ms.find("{1}"); at != std::string::npos) ms.replace(at, 3, std::to_string(rtt));
            net = ms;
            rgb = level == netwatch::Level::Good ? 0x5FD08A : level == netwatch::Level::Fair ? 0xF0A050 : 0xFF7D6E;
        }
        if (net != s.net || rgb != s.netRgb) {
            s.net = net;
            s.netRgb = rgb;
            s.dirty = true;
        }
        // 3.0.11-beta.127: its plate's graph - a new reading each second
        std::vector<int> graph = net.empty() ? std::vector<int>{} : netwatchwin::graph();
        if (graph != s.netGraph) {
            if (s.netGraph.empty() && !graph.empty())
                LOG_INFO("minimap: the round trip's plate under the bar - %s, a graph of the last minute", net.c_str());
            s.netGraph = std::move(graph);
            s.dirty = true;
        }
    }
    if (s.style != uistyle::generation()) {  // 3.0.11-beta.47: its bar's ground another opacity
        s.style = uistyle::generation();
        s.dirty = true;
    }
    if (uaui::ready() != s.paintedByUi) {  // 3.0.18-beta.26: painted for the other way - again, at once
        s.paintedByUi = uaui::ready();
        s.dirty = true;
        s.paintedAt = 0;
    }
    // 3.0.11-beta.57: at most each kPaintEveryMs (the first at once)
    if (s.dirty && (s.paintedAt == 0 || s.picture.width <= 0 || now - s.paintedAt >= kPaintEveryMs)) render(s);
    else  // 3.0.11-beta.75: known to «Розміщення інтерфейсу» each frame (painted or not)
        hudedit::place(hudlayout::Part::Minimap, s.k, s.targetW, s.targetH, s.layout.w, s.layout.h, s.layout.x,
                       s.layout.y);
    if (s.big && s.bigDirty && (s.bigPicture.width <= 0 || now - s.bigPaintedAt >= kPaintEveryMs)) renderBig(s);
    s.shown = true;
    block(s, game);
}

namespace {

// 3.0.18-beta.26 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 4 - by hand): the minimap's frame in
// RmlUi's markup - the map's square a picture of the driver's (pic:), its edge and the concept's gold corners, the
// buttons (+, −, the big map), the layers' round ones and «Схема», the bar (name, cell, ▾), the search's and the round
// trip's plates (the graph's bars), the way's bar, the monsters' list, their words where core minimap places them;
// drawn only (passive): its clicks, wheel, drag and blocker stay this part's
const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="mm" data-model="minimap" data-style-width="w + 'px'" data-style-height="h + 'px'" data-style-opacity="op">
<img class="a" data-attr-src="square" style="left: 0px; top: 0px;" data-style-width="side + 'px'"
  data-style-height="side + 'px'"/>
<div data-for="b : cards" data-attr-class="'card ' + b.cls" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'"
  data-style-width="b.w + 'px'" data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'"
  data-style-border-width="b.e + 'px'" data-style-background-color="ground"></div>
<div data-for="b : boxes" data-attr-class="b.cls" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'"
  data-style-width="b.w + 'px'" data-style-height="b.h + 'px'" data-style-border-radius="b.r + 'px'"
  data-style-border-width="b.e + 'px'"></div>
<div class="a" data-for="b : bars" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'"
  data-style-width="b.w + 'px'" data-style-height="b.h + 'px'" data-style-background-color="b.cls"></div>
<div class="a" data-for="i : icons">
 <img class="a" data-attr-src="i.src" data-style-left="i.x + 'px'" data-style-top="i.y + 'px'"
  data-style-width="i.w + 'px'" data-style-height="i.h + 'px'"/></div>
<span data-for="t : words" data-attr-class="t.cls" data-style-left="t.x + 'px'" data-style-top="t.y + 'px'"
  data-style-width="t.w + 'px'" data-style-height="t.h + 'px'" data-style-line-height="t.h + 'px'"
  data-style-font-size="t.px + 'px'" data-style-color="t.colour">{{t.text}}</span>
</body></rml>)RML";

struct BoxV {
    float x = 0, y = 0, w = 0, h = 0, r = 0, e = 0;
    Rml::String cls;  // a bar's: its colour
};
struct IconV {
    float x = 0, y = 0, w = 0, h = 0;
    Rml::String src;
};
struct WordV {
    float x = 0, y = 0, w = 0, h = 0, px = 12;
    Rml::String text, colour = "#eceff5", cls;
};

// 3.0.18-beta.175 (R-6PYQQG: the minimap said all its data again up to 15 times a second - its map's picture new
// each time, its buttons and words mostly the same): the lists compared, only the changed ones said to RmlUi
bool operator==(const BoxV& a, const BoxV& b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h && a.r == b.r && a.e == b.e && a.cls == b.cls;
}
bool operator==(const IconV& a, const IconV& b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h && a.src == b.src;
}
bool operator==(const WordV& a, const WordV& b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h && a.px == b.px && a.text == b.text &&
           a.colour == b.colour && a.cls == b.cls;
}

struct Ui {
    uaui::Doc doc;
    float w = 0, h = 0, side = 0, op = 1;
    Rml::String square, ground = "#00000000";  // 3.0.18-beta.76: never an empty style value
    std::vector<BoxV> cards, boxes, bars;
    std::vector<IconV> icons;
    std::vector<WordV> words;
    int gen = -1;
    std::map<std::string, panel::Picture> made;
    std::map<std::string, std::string> sources;
    std::string squareSrc;
};

Ui& ui() {
    static Ui u;
    return u;
}

std::string hexOf(uint32_t rgb) {
    char b[16];
    std::snprintf(b, sizeof b, "#%06x", unsigned(uaui::themed(rgb) & 0xFFFFFF));
    return b;
}

template <typename F>
std::string madeOnce(Ui& u, const std::string& key, int w, int h, F paint) {
    auto it = u.made.find(key);
    if (it == u.made.end()) {
        panel::Picture p;
        p.width = std::max(1, w), p.height = std::max(1, h);
        p.px.assign(size_t(p.width) * size_t(p.height), 0u);
        paint(p);
        it = u.made.emplace(key, std::move(p)).first;
    }
    return uaui::picture(&it->second, uaui::stamp(&it->second), u.sources[key]);
}

bool uiFrame(State& s) {
    Ui& u = ui();
    if (!uaui::ready() || !s.shown || s.look == minimap::Look::Hidden || s.square.width <= 0 || s.layout.w <= 0) {
        u.doc.show(false);
        return false;
    }
    if (!u.doc.made()) {
        u.doc.passive = true;
        if (!u.doc.make("minimap", kDoc, [&](Rml::DataModelConstructor& m) {
                if (auto b = m.RegisterStruct<BoxV>()) {
                    b.RegisterMember("x", &BoxV::x);
                    b.RegisterMember("y", &BoxV::y);
                    b.RegisterMember("w", &BoxV::w);
                    b.RegisterMember("h", &BoxV::h);
                    b.RegisterMember("r", &BoxV::r);
                    b.RegisterMember("e", &BoxV::e);
                    b.RegisterMember("cls", &BoxV::cls);
                }
                if (auto i = m.RegisterStruct<IconV>()) {
                    i.RegisterMember("x", &IconV::x);
                    i.RegisterMember("y", &IconV::y);
                    i.RegisterMember("w", &IconV::w);
                    i.RegisterMember("h", &IconV::h);
                    i.RegisterMember("src", &IconV::src);
                }
                if (auto t = m.RegisterStruct<WordV>()) {
                    t.RegisterMember("x", &WordV::x);
                    t.RegisterMember("y", &WordV::y);
                    t.RegisterMember("w", &WordV::w);
                    t.RegisterMember("h", &WordV::h);
                    t.RegisterMember("px", &WordV::px);
                    t.RegisterMember("text", &WordV::text);
                    t.RegisterMember("colour", &WordV::colour);
                    t.RegisterMember("cls", &WordV::cls);
                }
                m.RegisterArray<std::vector<BoxV>>();
                m.RegisterArray<std::vector<IconV>>();
                m.RegisterArray<std::vector<WordV>>();
                m.Bind("w", &u.w);
                m.Bind("h", &u.h);
                m.Bind("side", &u.side);
                m.Bind("op", &u.op);
                m.Bind("square", &u.square);
                m.Bind("ground", &u.ground);
                m.Bind("cards", &u.cards);
                m.Bind("boxes", &u.boxes);
                m.Bind("bars", &u.bars);
                m.Bind("icons", &u.icons);
                m.Bind("words", &u.words);
            }))
            return false;
    }
    const float op = s.look == minimap::Look::Faded ? minimap::opacity(s.look) : 1.0f;  // Ctrl+Tab's see-through
    if (s.uiGen != u.gen || op != u.op) {
        u.gen = s.uiGen;
        // 3.0.18-beta.175: what it said before - only what changed said again
        const float wasW = u.w, wasH = u.h, wasSide = u.side, wasOp = u.op;
        const Rml::String wasSquare = u.square, wasGround = u.ground;
        const std::vector<BoxV> wasCards = u.cards, wasBoxes = u.boxes, wasBars = u.bars;
        const std::vector<IconV> wasIcons = u.icons;
        const std::vector<WordV> wasWords = u.words;
        const minimap::Layout& L = s.layout;
        const float sc = L.s, edge = std::max(1.0f, sc);
        u.w = float(L.w), u.h = float(L.h), u.side = L.map.w, u.op = op;
        u.square = uaui::picture(&s.square, s.squareGen, u.squareSrc);
        u.ground = hexOf(0x10141C) + [] {
            char a[4];
            std::snprintf(a, sizeof a, "%02x", int(std::lround(uistyle::panel() * 255)) & 0xFF);
            return std::string(a);
        }();
        const minimap::Part hover = s.part;
        u.cards.clear(), u.boxes.clear(), u.bars.clear(), u.icons.clear();
        const auto box = [&](std::vector<BoxV>& to, const panel::Box& b, float r, const std::string& cls, float e = 0) {
            to.push_back(BoxV{b.x, b.y, b.w, b.h, r, e, cls});
        };
        box(u.boxes, L.map, 10 * sc, "edge", edge);
        const float arm = 15 * sc, thick = std::max(1.5f, 2 * sc);  // the concept's gold corners
        for (int c = 0; c < 4; ++c) {
            const float cx = c & 1 ? L.map.w : 0, cy = c & 2 ? L.map.h : 0;
            const bool right = c & 1, bottom = c & 2;
            box(u.boxes, panel::Box{right ? cx - arm : cx, bottom ? cy - thick : cy, arm, thick}, 0, "gold");
            box(u.boxes, panel::Box{right ? cx - thick : cx, bottom ? cy - arm : cy, thick, arm}, 0, "gold");
        }
        const int icon = std::max(8, int(std::ceil(14 * sc * L.grow)));  // .51: grown with the buttons
        const std::pair<const panel::Box*, uiicons::Icon> buttons[] = {{&L.zoomIn, uiicons::Icon::Plus},
                                                                      {&L.zoomOut, uiicons::Icon::Minus},
                                                                      {&L.expand, uiicons::Icon::Expand}};
        const minimap::Part parts[] = {minimap::Part::ZoomIn, minimap::Part::ZoomOut, minimap::Part::Expand};
        for (int i = 0; i < 3; ++i) {
            const panel::Box& b = *buttons[i].first;
            box(u.boxes, b, 6 * sc, hover == parts[i] ? "btn hot" : "btn", edge);
            const std::string src = madeOnce(u, "i" + std::to_string(i) + ':' + std::to_string(icon), icon, icon,
                                             [&](panel::Picture& p) {
                                                 uiicons::draw(p, buttons[i].second, 0, 0, float(icon), 0xFFFFFF,
                                                               1.0f, 2.6f);
                                             });
            u.icons.push_back(IconV{std::round(b.x + (b.w - float(icon)) / 2),
                                    std::round(b.y + (b.h - float(icon)) / 2), float(icon), float(icon), src});
        }
        const minimap::Layers layers = layersOf(s.show);
        const panel::Picture* face = s.kafraFace.width > 0 ? &s.kafraFace : nullptr;
        for (int i = 0; i < minimap::kLayers; ++i) {  // the layers' round buttons, each its marks' sign
            const panel::Box& b = L.layers[i];
            if (b.w <= 0) continue;
            const bool over = hover == minimap::Part::Layer && s.hoverLayer == i;
            box(u.boxes, b, b.w / 2, over ? "btn hot" : "btn", edge);
            const int side = int(std::ceil(b.w));
            const bool on = layers.on[i];
            const std::string key = "l" + std::to_string(i) + (on ? "+" : "-") + std::to_string(side) +
                                    (i == int(minimap::Layer::Kafra) && face ? ":" + std::to_string(uaui::stamp(face))
                                                                             : std::string());
            const std::string src = madeOnce(u, key, side, side, [&](panel::Picture& p) {
                minimap::paintLayerSign(p, minimap::Layer(i), on, face, sc * L.grow);
            });
            u.icons.push_back(IconV{b.x, b.y, float(side), float(side), src});
        }
        if (L.style.w > 0) {  // «Схема»: lit gold while it shows
            const panel::Box& b = L.style;
            const bool scheme = s.show.scheme && !s.scheme.levels.empty(), over = hover == minimap::Part::Style;
            box(u.boxes, b, b.w / 2, scheme ? "btn scheme" : over ? "btn hot" : "btn", edge);
            const int side = int(std::ceil(b.w));
            u.icons.push_back(IconV{b.x, b.y, float(side), float(side),
                                    madeOnce(u, "scheme" + std::to_string(side), side, side,
                                             [&](panel::Picture& p) { minimap::paintSchemeSign(p); })});
        }
        box(u.cards, L.caption, 8 * sc, "bar", edge);  // the bar: name, cell, ▾
        if (hover == minimap::Part::Fold) box(u.boxes, L.fold, 6 * sc, "hot");
        if (L.found.w > 0) {  // a search's plate
            box(u.cards, L.found, L.found.h / 2, "bar", edge);
            box(u.boxes, L.foundClear, L.foundClear.h / 2, "clear");
        }
        if (L.net.w > 0) {  // the round trip's plate: its ground, the graph's baseline and bars
            box(u.cards, L.net, L.net.h / 2, "bar", edge);
            const panel::Box g = minimap::netGraphBox(L);
            const float line = std::max(1.0f, std::round(sc));
            box(u.boxes, panel::Box{g.x, g.y + g.h - line, g.w, line}, 0, "base");
            for (const minimap::NetBar& b : s.uiBars) box(u.bars, b.box, 0, hexOf(b.rgb) + "f2");
        }
        if (L.route.w > 0) {  // the way's bar: gold-tinted, its ×
            box(u.cards, L.route, 8 * sc, "route");
            box(u.boxes, L.route, 8 * sc, "route", edge);
            if (hover == minimap::Part::Stop) box(u.boxes, L.stop, 5 * sc, "hot");
        }
        if (L.list.w > 0) box(u.cards, L.list, 8 * sc, "list", edge);
        u.words.clear();
        for (const minimap::Words& w : s.uiWords) {
            const char* align = w.align == minimap::Align::Right    ? " right"
                                : w.align == minimap::Align::Center ? " centre"
                                                                    : "";
            u.words.push_back(WordV{w.box.x, w.box.y, w.box.w, w.box.h, w.px, w.text, hexOf(w.rgb),
                                    std::string(w.bold ? "word bold" : "word") + align});
        }
        if (u.w != wasW) u.doc.dirty("w");
        if (u.h != wasH) u.doc.dirty("h");
        if (u.side != wasSide) u.doc.dirty("side");
        if (u.op != wasOp) u.doc.dirty("op");
        if (u.square != wasSquare) u.doc.dirty("square");
        if (u.ground != wasGround) u.doc.dirty("ground");
        if (u.cards != wasCards) u.doc.dirty("cards");
        if (u.boxes != wasBoxes) u.doc.dirty("boxes");
        if (u.bars != wasBars) u.doc.dirty("bars");
        if (u.icons != wasIcons) u.doc.dirty("icons");
        if (u.words != wasWords) u.doc.dirty("words");
    }
    u.doc.place(s.layout.x, s.layout.y);
    u.doc.show(true);
    return true;
}

}  // namespace

// 3.0.18-beta.42 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the big map in RmlUi's
// markup, as core minimap's paintBig - its card, the header's line, ✕ lit, the map's square (a picture: the map, the
// marks, the labels' grounds), the layers' chips with their signs, «Схема»; the words where core minimap places them
static void bigSheet(State& s, uisheet::Content& c) {
    const minimap::Big& b = s.bigLayout;
    const float sc = b.s, edge = std::max(1.0f, sc);
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kDark = 0x0B0E14, kAccent = 0xC9A55B;
    c.w = float(b.w), c.h = float(b.h);
    c.rect(c.under, panel::Box{0, 0, float(b.w), float(b.h)}, 12 * sc, kCard, 0.96f, kWhite, 0.14f, edge);
    c.rect(c.under, panel::Box{0, b.header.h - edge, float(b.w), edge}, 0, kWhite, 0.10f);
    if (s.bigPart == minimap::BigPart::Close) c.rect(c.under, b.close, 9 * sc, kWhite, 0.10f);
    c.icons.push_back(uisheet::Icon{b.map.x, b.map.y, float(s.bigSquare.width), float(s.bigSquare.height), 1,
                                    s.bigSheet.icon(&s.bigSquare)});
    const minimap::Layers layers = layersOf(s.show);
    const panel::Picture* face = s.kafraFace.width > 0 ? &s.kafraFace : nullptr;
    for (int i = 0; i < minimap::kLayers; ++i) {
        const panel::Box& chip = b.chips[i];
        if (chip.w <= 0) continue;
        const bool on = layers.on[i], over = s.bigPart == minimap::BigPart::Layer && s.hoverLayer == i;
        c.rect(c.under, chip, chip.h / 2, on ? kAccent : kDark, on ? (over ? 0.32f : 0.22f) : (over ? 0.9f : 0.7f),
               on ? kAccent : kWhite, on ? 0.7f : 0.16f, edge);
        const int side = std::max(4, int(std::ceil(chip.h)));
        const std::string key = "big" + std::to_string(i) + (on ? "+" : "-") + std::to_string(side) +
                                (i == int(minimap::Layer::Kafra) && face ? ":" + std::to_string(uaui::stamp(face))
                                                                         : std::string());
        const std::string src = s.bigSheet.made(key, side, side, [&](panel::Picture& p) {
            minimap::paintLayerSign(p, minimap::Layer(i), on, face, sc * 6.0f / 5.5f);
        });
        c.icons.push_back(uisheet::Icon{chip.x, chip.y, float(side), float(side), 1, src});
    }
    if (b.styleChip.w > 0) {
        const panel::Box& chip = b.styleChip;
        const bool scheme = s.show.scheme && !s.scheme.levels.empty(), over = s.bigPart == minimap::BigPart::Style;
        c.rect(c.under, chip, chip.h / 2, scheme ? kAccent : kDark,
               scheme ? (over ? 0.42f : 0.32f) : (over ? 0.9f : 0.7f), scheme ? kAccent : kWhite, scheme ? 0.8f : 0.16f,
               edge);
        const int side = std::max(4, int(std::ceil(chip.h)));
        c.icons.push_back(uisheet::Icon{chip.x, chip.y, float(side), float(side), 1,
                                        s.bigSheet.made("bigscheme" + std::to_string(side), side, side,
                                                        [&](panel::Picture& p) { minimap::paintSchemeSign(p); })});
    }
    c.say(s.bigUiWords);
}

// 3.0.18-beta.133: the pad's cross over the big map - a gold ring at its cell, drawn by uaUI over the map's sheet
static void crossFrame() {
    static uaui::Doc doc;
    static float side = 0;
    int x = 0, y = 0;
    float sd = 0;
    const bool on = uaui::ready() && padCross(x, y, sd);
    if (!on) {
        doc.show(false);
        return;
    }
    if (!doc.made()) {
        doc.passive = true;
        doc.top = true;
        if (!doc.make("big map cross", R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="bmx" data-model="bigcross" data-style-width="side + 'px'" data-style-height="side + 'px'">
<div class="r" data-style-width="side + 'px'" data-style-height="side + 'px'"
  data-style-border-radius="side / 2 + 'px'"></div><div class="d"></div></body></rml>)RML",
                      [&](Rml::DataModelConstructor& m) { m.Bind("side", &side); }))
            return;
    }
    if (sd != side) side = sd, doc.dirtyAll();
    doc.place(x - int(sd / 2), y - int(sd / 2));
    doc.show(true);
}

void frame(HWND game, const Show& show, double k, int targetWidth, int targetHeight) {
    frameInner(game, show, k, targetWidth, targetHeight);
    State& s = state();
    s.uiShown = uiFrame(s);
    if (uaui::ready() != s.bigByUi) {  // painted for the other way: again
        s.bigByUi = uaui::ready();
        s.bigDirty = true;
    }
    s.bigShown = s.bigSheet.frame("big map", s.shown && s.big && s.bigSquare.width > 0 && s.bigGen > 0, true,
                                  s.bigLayout.x, s.bigLayout.y, s.bigGen, [&](uisheet::Content& c) { bigSheet(s, c); });
    crossFrame();  // 3.0.18-beta.133
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
    (void)record;  // the old edition (its picture) gone: uaUI draws it
    (void)renderer;
}

// 3.0.11-beta.201 (the tester: «мапа має перекривати всі»): the big map apart - drawn after the panels (the chat, the
// hotbar, the character's), under the windows
void drawBig(FrameRecord& record, gpu::Renderer& renderer, bool over) {
    State& s = state();
    if (!over) {  // 3.0.11-beta.260: a window opened (another top of their stack) - the map under them again
        static std::optional<winstack::Win> lastTop;
        const std::optional<winstack::Win> top = windowstack::top();
        if (top != lastTop && top && s.bigTop) {
            s.bigTop = false;
            LOG_INFO("minimap: the big map under the windows (one opened)");
        }
        lastTop = top;
    }
    if (over != s.bigTop) return;
    if (!s.shown || s.k <= 0 || !s.big || s.bigShown || s.bigPicture.width <= 0) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    drawPart(record, renderer, s.bigPicture, s.bigTexture, s.bigTexW, s.bigTexH, s.bigUpload, s.bigLayout.x,
             s.bigLayout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool bigOnTop() {
    const State& s = state();
    return s.shown && s.big && s.bigTop;
}

void lowerBig() {
    State& s = state();
    if (!s.bigTop) return;
    s.bigTop = false;
    if (s.big) LOG_INFO("minimap: the big map under the windows (a press in one)");
}

namespace {
bool mouseOf(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result, bool bigOnly);
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) { return mouseOf(game, m, wp, lp, result, false); }

bool mouseBig(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    return bigOnTop() && mouseOf(game, m, wp, lp, result, true);
}

namespace {
bool mouseOf(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result, bool bigOnly) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = mouseButton(m);
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
            if ((m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN) && !s.bigTop) {  // 3.0.11-beta.260: raised
                s.bigTop = true;
                LOG_INFO("minimap: the big map over the windows (pressed)");
            }
            if (m == WM_LBUTTONDOWN && bp == minimap::BigPart::Close) setBig(s, false);
            if (m == WM_LBUTTONDOWN && bp == minimap::BigPart::Layer)  // 3.0.11-beta.119: a layer on or off
                switchLayer(s, minimap::bigLayerAt(s.bigLayout, bx, by));
            if (m == WM_LBUTTONDOWN && bp == minimap::BigPart::Style) switchStyle(s);  // 3.0.11-beta.120
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
    if (bigOnly || s.look == minimap::Look::Hidden) return false;
    const float px = tx - float(s.layout.x), py = ty - float(s.layout.y);
    const minimap::Part part = minimap::partAt(s.layout, px, py);
    if (part == minimap::Part::None) return false;
    if (m == WM_MOUSEWHEEL) {  // 3.0.11-beta.31: the wheel over it zooms
        if (part == minimap::Part::Map) setZoom(s, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1);
        result = 0;
        return true;
    }
    if (m == WM_LBUTTONDOWN && s.clicks++ == 0)  // the check of the prototype: the character did not walk
        LOG_INFO("minimap: a click on the new minimap kept from the game (the player at %d, %d on %s)", s.cell.x,
                 s.cell.y, s.map.c_str());
    if (m == WM_LBUTTONDOWN) {
        switch (part) {
            case minimap::Part::FoundClear:  // 3.0.11-beta.213: @mobsearch's marks cleared
                LOG_INFO("minimap: @mobsearch's %zu marks cleared", s.found.result().points.size());
                s.found.clear();
                s.othersKey.clear();
                s.dirty = s.bigDirty = true;
                break;
            case minimap::Part::Found: break;
            case minimap::Part::Layer: switchLayer(s, minimap::layerAt(s.layout, px, py)); break;  // .119
            case minimap::Part::Style: switchStyle(s); break;                                      // .120
            case minimap::Part::ZoomIn: setZoom(s, -1); break;  // fewer cells: nearer
            case minimap::Part::ZoomOut: setZoom(s, 1); break;
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
}  // namespace

bool showBig(bool open) {
    State& s = state();
    if (!s.on) return false;
    setBig(s, open);
    return true;
}

bool bigShown() { return state().on && state().big; }

bool canFollow() { return state().on; }

bool layerShown(int layer) {
    State& s = state();
    if (layer < 0 || layer >= minimap::kLayers) return false;
    return layersOf(withWanted(s, s.show, GetTickCount64())).on[layer];
}

void switchLayerFromReference(int layer) { switchLayer(state(), layer); }

bool schemeShown() {
    State& s = state();
    return withWanted(s, s.show, GetTickCount64()).scheme;
}

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

void heard(const std::string& line, unsigned long long ms) {
    State& s = state();
    const size_t before = s.found.result().points.size();
    const bool fresh = line.compare(0, 14, "Mob Search... ") == 0;
    if (!s.found.line(line, ms)) return;
    if (fresh) LOG_INFO("minimap: @mobsearch «%s» on %s", s.found.result().what.c_str(), s.found.result().map.c_str());
    if (s.found.result().points.size() != before || fresh) {
        s.othersKey.clear();
        s.dirty = s.bigDirty = true;
    }
}

void packet(const packets::Event& e) {
    State& s = state();
    if (const auto* v = std::get_if<packets::Viewpoint>(&e)) {  // 3.0.11-beta.201: the server's mark
        const std::pair<uint32_t, int> key{v->npc, v->id};
        if (v->type == 2 || (v->x == 0 && v->y == 0)) {
            s.spots.erase(key);
        } else {
            s.spots[key] = State::Spot{v->x, v->y, v->colour};
            static int told = 0;
            if (told++ < 5)
                LOG_INFO("minimap: the server's mark %d of %u at %d, %d (#%06X)", v->id, v->npc, v->x, v->y,
                         unsigned(v->colour));
        }
    } else if (const auto* g = std::get_if<packets::GuildPos>(&e)) {  // 3.0.11-beta.201: a guild mate's cell
        uint32_t own = 0;
        readGame(kOwnAccount, &own, sizeof own);
        if (g->account != own) s.mates[g->account] = State::Mate{g->x, g->y, GetTickCount64()};
    }
}

bool padBig(const BigPad& p) {
    State& s = state();
    if (!s.shown || !s.big || s.size.w <= 0 || s.size.h <= 0) {
        s.padCross = false;
        return false;
    }
    if (!s.padCross) {  // from the character's own cell
        s.padCross = true;
        s.padCell = s.cellKnown ? minimap::Cell{s.cell.x, s.cell.y} : minimap::Cell{s.size.w / 2, s.size.h / 2};
        s.padFx = s.padFy = 0;
    }
    const float mag = std::hypot(p.lx, p.ly);
    if (mag > 0.2f) {  // 12 to 60 cells a second by how far the stick is pushed
        const float speed = (12.0f + 48.0f * std::min(1.0f, (mag - 0.2f) / 0.8f)) * p.seconds;
        s.padFx += p.lx / mag * speed;
        s.padFy += p.ly / mag * speed;  // the stick's up is north
        const int dx = int(s.padFx), dy = int(s.padFy);
        s.padFx -= float(dx), s.padFy -= float(dy);
        s.padCell.x = std::clamp(s.padCell.x + dx, 0, s.size.w - 1);
        s.padCell.y = std::clamp(s.padCell.y + dy, 0, s.size.h - 1);
    }
    if (p.next && !s.marks.empty()) {  // the next NPC, warp or Kafra round the map
        std::vector<size_t> order;
        for (size_t i = 0; i < s.marks.size(); ++i)
            if (s.marks[i].glyph != minimap::Glyph::Member) order.push_back(i);
        if (!order.empty()) {
            size_t at = 0;
            for (size_t k = 0; k < order.size(); ++k)
                if (s.marks[order[k]].cell.x == s.padCell.x && s.marks[order[k]].cell.y == s.padCell.y)
                    at = (k + 1) % order.size();
            s.padCell = s.marks[order[at]].cell;
        }
    }
    if (p.plot) {
        setPlot(s, s.padCell, false);
        plotFrame(s);
        LOG_INFO("minimap: the way to %d, %d (the pad's cross)", s.padCell.x, s.padCell.y);
    }
    if (p.clear) clearPlot(s);
    if (p.close) {
        s.padCross = false;
        setBig(s, false);
    }
    return true;
}

bool padCross(int& x, int& y, float& side) {
    const State& s = state();
    if (!s.big || !s.padCross || s.size.w <= 0) return false;
    const float cellPx = s.bigLayout.map.w / float(s.size.w);
    x = int(float(s.bigLayout.x) + s.bigLayout.map.x + (float(s.padCell.x) + 0.5f) * cellPx);
    y = int(float(s.bigLayout.y) + s.bigLayout.map.y + (float(s.size.h) - (float(s.padCell.y) + 0.5f)) * cellPx);
    side = std::max(14.0f, 22.0f * s.scale);
    return true;
}

}  // namespace uagfx::win::minimapproto
