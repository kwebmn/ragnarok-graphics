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
#include "core/walk_path.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/ground_way.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/reference_window.hpp"

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
    minimap::Look look = minimap::Look::Opaque;  // 3.0.11-beta.4: Ctrl+Tab's round
    bool hidOld = false;      // the game's own window 14 deleted by us (the game makes it again on a new map)
    bool hideFailed = false;  // told once a map
    std::string map;
    std::vector<uint32_t> argb;  // the map's own minimap picture (the game's), the whole map
    int picW = 0, picH = 0;
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
    // 3.0.11-beta.31 (the owner: «нету возможности карту развернуть на весь екран»): the map big in the middle
    bool big = false, bigDirty = true, bigUpload = true;
    minimap::Big bigLayout;
    minimap::BigPart bigPart = minimap::BigPart::None;
    panel::Picture bigMap, bigPicture;  // its map's picture alone (made once a map and a size), and all of it
    std::string bigMapOf;
    uint32_t bigTexture = 0;
    int bigTexW = 0, bigTexH = 0;
};

State& state() {
    static State s;
    return s;
}

void loadPicture(State& s) {
    s.argb.clear();
    s.picW = s.picH = 0;
    if (!s.files) s.files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    std::string from;
    const std::string entry = "data\\texture\\" + kUi + "\\map\\" + s.map + ".bmp";
    if (s.files->read(entry, bytes, &from) && pack::decodeBmp(bytes.data(), bytes.size(), s.argb, s.picW, s.picH)) {
        LOG_INFO("minimap: %s's picture %dx%d from %s", s.map.c_str(), s.picW, s.picH, from.c_str());
        return;
    }
    s.argb.clear();
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

// 3.0.11-beta.32: the way on the ground itself (win/ground_way) - the same cells as the minimap's line
void toGround(const State& s) {
    std::vector<std::pair<int, int>> cells;
    for (const minimap::Cell& c : s.path) cells.emplace_back(c.x, c.y);
    win::groundway::set(s.map, cells);
}

// 3.0.11-beta.31: the way over the walked cells to the next move's cell - its distances made once a goal, its cells
// from the player each time the player is on another cell; no cells known for the map: a straight line
void makePath(State& s) {
    const refroute::Step* step = s.cellKnown ? routefollow::next(s.follow, s.map) : nullptr;
    if (!step) {
        if (!s.path.empty()) s.dirty = true;
        s.path.clear();
        toGround(s);
        return;
    }
    const minimap::Cell goal{step->move.x, step->move.y};
    if (s.cellsMap != s.map) {
        s.cellsMap = s.map;
        s.cells = walkpath::Cells{};
        s.field = walkpath::Field{};
        if (s.index && s.index->walk) {
            const auto it = s.index->walk->find(s.map);
            if (it != s.index->walk->end()) s.cells = walkpath::cellsOf(it->second);
        }
    }
    const bool newGoal = s.field.goal != walkpath::Cell{goal.x, goal.y} || s.field.w != s.cells.w;
    if (newGoal && s.cells.w > 0) {
        const ULONGLONG t0 = GetTickCount64();
        s.field = walkpath::fieldTo(s.cells, walkpath::Cell{goal.x, goal.y});
        LOG_INFO("route: the walked cells of %s to %d, %d measured in %llu ms", s.map.c_str(), goal.x, goal.y,
                 static_cast<unsigned long long>(GetTickCount64() - t0));
    }
    if (!newGoal && s.pathFrom.x == s.cell.x && s.pathFrom.y == s.cell.y) return;
    s.pathFrom = s.cell;
    s.path.clear();
    if (s.cells.w > 0) {
        for (const walkpath::Cell& c : walkpath::straightened(
                 s.cells, walkpath::pathFrom(s.field, s.cells, walkpath::Cell{s.cell.x, s.cell.y})))
            s.path.push_back(minimap::Cell{c.first, c.second});
    }
    if (s.path.empty()) s.path = {s.cell, goal};  // no walked cells known (or none reach it): straight to it
    s.dirty = true;
    s.bigDirty = true;
    toGround(s);
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

minimap::Scene sceneOf(const State& s) {
    minimap::Scene sc;
    sc.argb = &s.argb;
    sc.imgW = s.picW;
    sc.imgH = s.picH;
    sc.map = s.size;
    sc.player = s.cell;
    sc.lookX = s.lookX;
    sc.lookY = s.lookY;
    sc.cells = minimap::cellsAt(s.zoom, s.size);
    sc.marks = s.marks;
    sc.path = s.path;
    if (const refroute::Step* step = s.cellKnown ? routefollow::next(s.follow, s.map) : nullptr) {
        sc.pin = true;
        sc.pinCell = minimap::Cell{step->move.x, step->move.y};
    }
    sc.hoverMark = s.hover;
    sc.hover = s.part;
    sc.bigHover = s.bigPart;
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
    t.route = s.routeLines;
    for (const Line& l : s.lines)
        t.list.push_back(minimap::Line{l.left, l.right, l.boss ? kBossText : 0u,
                                       l.timer ? minimap::kRouteRgb : 0u, l.boss});
    s.layout = minimap::layout(s.targetW, s.targetH, s.scale, !t.route.empty(), s.folded ? 0 : int(t.list.size()));
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
    const std::string key = s.map + "|" + std::to_string(s.bigLayout.w) + "x" + std::to_string(s.bigLayout.h) + "|" +
                            std::to_string(s.picW);
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
    if (!on) {
        if (s.shown) unblock();
        s.shown = false;
        s.big = false;
        win::groundway::set("", {});  // 3.0.11-beta.32: no way on the ground either
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
        loadPicture(s);
        s.dirty = s.bigDirty = true;
    }
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
    float east = 0, north = 0;  // 3.0.11-beta.31: the arrow turns as the camera does (by a degree or more)
    if (readLook(east, north) && (std::fabs(east - s.lookX) > 0.017f || std::fabs(north - s.lookY) > 0.017f)) {
        if (s.lookX == 0 && s.lookY == 0)  // the first a map: to set the arrow against the screen
            LOG_INFO("minimap: the camera looks %.2f east, %.2f north on %s (the arrow that way)", east, north,
                     s.map.c_str());
        s.lookX = east;
        s.lookY = north;
        s.dirty = s.bigDirty = true;
    }
    const float scale = std::clamp(float(k), 0.75f, 6.0f);
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
    followFrame(s);
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
    if (s.dirty) render(s);
    if (s.big && s.bigDirty) renderBig(s);
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
        drawPicture(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
        ++mine;
    }
    if (s.big && s.bigPicture.width > 0) {  // 3.0.11-beta.31: the big map over the rest of the game's picture
        drawPicture(record, renderer, s.bigPicture, s.bigTexture, s.bigTexW, s.bigTexH, s.bigUpload, s.bigLayout.x,
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
            if (m == WM_LBUTTONDOWN && bp == minimap::BigPart::Map) {  // a warp: its map's card in «Довідник»
                const int mark = minimap::bigMarkAt(s.bigLayout, sc, bx, by);
                if (mark >= 0 && s.index && s.marks[size_t(mark)].glyph == minimap::Glyph::Warp) {
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
            case minimap::Part::Stop: unfollow(); break;
            case minimap::Part::Caption: referencewin::openMap(s.map); break;  // 3.0.11-beta.7: its card
            case minimap::Part::Route: referencewin::openMap(s.follow.target); break;
            case minimap::Part::List: {  // 3.0.11-beta.9: a monster's card; «і ще N» - the map's
                const int i = minimap::lineAt(s.layout, py);
                if (i >= 0 && size_t(i) < s.lines.size()) {
                    if (s.lines[size_t(i)].mob > 0) referencewin::openMob(s.lines[size_t(i)].mob);
                    else referencewin::openMap(s.map);
                }
                break;
            }
            case minimap::Part::Map:
            case minimap::Part::None: break;
        }
    }
    result = 0;
    return true;
}

bool canFollow() { return state().on; }

void follow(const std::string& target, bool walk) {
    State& s = state();
    s.follow = routefollow::Follow{};
    s.follow.active = !target.empty();
    s.follow.target = target;
    s.follow.walk = walk;
    s.arrivedAt = 0;
    s.pathFrom = minimap::Cell{-1, -1};
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
