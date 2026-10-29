#include "win/minimap_proto.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/minimap_proto.hpp"
#include "core/pack_source.hpp"
#include "core/reference_index.hpp"
#include "core/reference_pages.hpp"
#include "core/route_follow.hpp"
#include "core/settings/strings.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/reference_window.hpp"

namespace uagfx::win::minimapproto {
namespace {

constexpr int kCells = 100;  // the view: 100 cells across, as the minimap test's cards
const std::string kUi = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";  // 유저인터페이스
constexpr size_t kMostLines = 8;            // 3.0.11-beta.6: the monsters under the map at most (the last «і ще N»)
constexpr int kBossRespawnMs = 600000;      // a boss's respawn told from 10 minutes (the site's MVP timers' bound)
constexpr uint32_t kBossText = 0xF5C451;    // a boss's name

struct Line {  // a monster under the map: its name, its count (a boss's respawn)
    std::wstring left, right;
    bool boss = false;
    int mob = 0;         // 3.0.11-beta.9: its id - a click opens its card (0: «і ще N», the map's card)
    bool route = false;  // 3.0.11-beta.9: the way's line (its colour; a click opens the target's card)
};
constexpr ULONGLONG kThereMs = 8000;  // 3.0.11-beta.9: «Ви на місці» shown so long, then the way ends

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
    minimap::Geometry geometry;
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
    std::vector<std::string> labels;  // the mark's words under the pointer
    std::vector<Line> lines;
    int hover = -1;
    std::map<std::string, std::string> mapNames;  // the client's data\mapnametable.txt
    bool namesRead = false;
    // 3.0.11-beta.9: the way followed - its lines first under the map
    routefollow::Follow follow;
    ULONGLONG arrivedAt = 0;
    std::vector<Line> routeLines;
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

std::wstring tr(settings::Text t) { return utf8Wide(settings::tr(startup().lang, t)); }

// 3.0.11-beta.6: the map's hints from the reference - marks with their words, the monsters' lines
void buildHints(State& s) {
    s.marks.clear();
    s.labels.clear();
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
        for (const refindex::Npc& n : h->npcs) {
            s.marks.push_back(minimap::Mark{minimap::Cell{n.x, n.y}, refindex::colorOf(n.kind), false});
            s.labels.push_back(n.name);
        }
    }
    if (h && s.show.warps) {
        readNames(s);
        for (const refindex::Warp& w : h->warps) {
            const auto name = s.mapNames.find(w.dest);
            s.marks.push_back(minimap::Mark{minimap::Cell{w.x, w.y}, 0, true});
            const std::string where =
                name != s.mapNames.end() ? refindex::shortName(name->second) + " (" + w.dest + ")" : w.dest;
            s.labels.push_back("\xE2\x86\x92 " + where);  // «→ Prontera Field (prt_fild07)»
        }
    }
    if (!s.show.mobs) return;
    if (!s.index) {
        s.lines.push_back(Line{tr(settings::Text::MinimapNoReference), L"", false});
        return;
    }
    if (!h || h->mobs.empty()) {
        s.lines.push_back(Line{tr(settings::Text::MinimapNoMonsters), L"", false});
        return;
    }
    const size_t room = h->mobs.size() > kMostLines ? kMostLines - 1 : h->mobs.size();
    for (size_t i = 0; i < room; ++i) {
        const refindex::MobOnMap& m = h->mobs[i];
        const auto mob = s.index->mobs.find(m.id);
        const std::string name = mob != s.index->mobs.end() ? mob->second.name : "#" + std::to_string(m.id);
        const bool boss = mob != s.index->mobs.end() && mob->second.boss;
        std::wstring right = std::to_wstring(m.count);
        if (boss && m.respawnMs >= kBossRespawnMs) right = utf8Wide(refpages::respawn(m.respawnMs, startup().lang));
        s.lines.push_back(Line{utf8Wide((boss ? "\xE2\x98\x85 " : "") + name), right, boss, m.id});
    }
    if (room < h->mobs.size())
        s.lines.push_back(Line{utf8Wide(settings::fmt(settings::tr(startup().lang, settings::Text::MinimapMore),
                                                      {std::to_string(h->mobs.size() - room)})), L"", false});
}

// 3.0.11-beta.9: a map's short name for the way's line: the client's, else its code
std::string shortTitle(State& s, const std::string& code) {
    readNames(s);
    const auto name = s.mapNames.find(code);
    const std::string short_ = name == s.mapNames.end() ? "" : refindex::shortName(name->second);
    return short_.empty() ? code : short_;
}

void buildRoute(State& s) {
    s.routeLines.clear();
    if (!s.follow.active) return;
    const std::string to = shortTitle(s, s.follow.target);
    if (s.map == s.follow.target) {
        s.routeLines.push_back(Line{tr(settings::Text::MinimapRouteThere), L"", true, 0, true});
        s.routeLines.push_back(Line{utf8Wide(to), L"", false, 0, true});
        return;
    }
    const refroute::Step* step = routefollow::next(s.follow, s.map);
    if (!step) {
        s.routeLines.push_back(Line{tr(settings::Text::MinimapRouteNone), L"", false, 0, true});
        s.routeLines.push_back(Line{utf8Wide("\xE2\x86\x92 " + to), L"", false, 0, true});
        return;
    }
    const std::string left = settings::fmt(settings::tr(startup().lang, settings::Text::MinimapRouteLeft),
                                           {std::to_string(s.follow.route.steps.size())});
    s.routeLines.push_back(Line{utf8Wide("\xE2\x86\x92 " + shortTitle(s, step->move.to)), utf8Wide(left), false, 0,
                                true});
    s.routeLines.push_back(Line{utf8Wide(refpages::stepNote(*step, startup().lang)), L"", false, 0, true});
}

// The way followed now on this map: made again when the player went elsewhere; «Ви на місці» a while, then the end
void followFrame(State& s) {
    if (!s.follow.active || !s.index) return;
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
        s.dirty = true;
    }
}

void render(State& s) {
    std::vector<Line> lines = s.routeLines;  // 3.0.11-beta.9: the way first, then the monsters
    lines.insert(lines.end(), s.lines.begin(), s.lines.end());
    s.geometry = minimap::geometry(s.targetW, s.targetH, s.scale, int(lines.size()));
    minimap::Goal goal;
    if (const refroute::Step* step = routefollow::next(s.follow, s.map); step && s.cellKnown)
        goal = minimap::Goal{true, minimap::Cell{step->move.x, step->move.y}};
    minimap::paint(s.picture, s.geometry, s.argb, s.picW, s.picH, s.size, s.cell, kCells, s.marks, goal);
    char text[96];
    if (s.cellKnown)
        std::snprintf(text, sizeof text, "%s  %d, %d", s.map.c_str(), s.cell.x, s.cell.y);
    else
        std::snprintf(text, sizeof text, "%s", s.map.c_str());
    const float sc = s.scale;
    const minimap::Geometry& g = s.geometry;
    const bool hovered = s.hover >= 0 && size_t(s.hover) < s.labels.size();  // 3.0.11-beta.6: the mark's words
    std::vector<Words> words{Words{panel::Box{4 * sc, float(g.size), float(g.size) - 8 * sc, float(g.textH)},
                                   utf8Wide(hovered ? s.labels[size_t(s.hover)] : std::string(text)),
                                   hovered ? kBossText : panel::kText, 12.5f * sc, hovered, DT_CENTER}};
    const float split = (float(g.size) - 8 * sc) * 0.68f;
    for (size_t i = 0; i < lines.size(); ++i) {  // the map's monsters: the name left, the count right
        const float y = float(g.size + g.textH) + float(i) * float(g.lineH);
        const Line& l = lines[i];
        const uint32_t colour = l.route ? (l.boss ? kBossText : minimap::kRouteRgb) : l.boss ? kBossText : panel::kText;
        words.push_back(Words{panel::Box{4 * sc, y, l.right.empty() ? float(g.size) - 8 * sc : split, float(g.lineH)},
                              l.left, colour, 11.5f * sc, l.boss, DT_LEFT});
        if (!l.right.empty())
            words.push_back(Words{panel::Box{4 * sc + split, y, float(g.size) - 8 * sc - split, float(g.lineH)},
                                  l.right, panel::kMuted, 11.5f * sc, false, DT_RIGHT});
    }
    drawWords(s.picture, words);
    if (s.look == minimap::Look::Faded) panel::fade(s.picture, minimap::opacity(s.look));  // Ctrl+Tab's see-through
    s.dirty = false;
    s.upload = true;
}

bool toTarget(const State& s, HWND game, int x, int y, float& tx, float& ty) {
    RECT client{};
    if (!GetClientRect(game, &client) || client.right <= 0 || client.bottom <= 0) return false;
    tx = float(x) * float(s.targetW) / float(client.right);  // Windows' size of the window (uaRO.exe is not DPI-aware)
    ty = float(y) * float(s.targetH) / float(client.bottom);
    return true;
}

void block(const State& s, HWND game) {  // the mouse's clicks there are the panel's, not the game's
    RECT client{};
    if (!GetClientRect(game, &client) || s.targetW <= 0 || s.targetH <= 0) return;
    const minimap::Geometry& g = s.geometry;
    const RECT r{LONG(int64_t(g.x) * client.right / s.targetW), LONG(int64_t(g.y) * client.bottom / s.targetH),
                 LONG(int64_t(g.x + g.size) * client.right / s.targetW + 1),
                 LONG(int64_t(g.y + g.size + g.textH + g.listH) * client.bottom / s.targetH + 1)};  // and its list
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Minimap, &r, false);
}

void unblock() { uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Minimap, nullptr, false); }

}  // namespace

void frame(HWND game, const Show& show, double k, int targetWidth, int targetHeight) {
    State& s = state();
    const bool on = show.on;
    if (!on) {
        if (s.shown) unblock();
        s.shown = false;
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
        loadPicture(s);
        s.dirty = true;
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
        if (!s.cellKnown || x != s.cell.x || y != s.cell.y || w != s.size.w || h != s.size.h) s.dirty = true;
        s.cell = minimap::Cell{x, y};
        s.size = minimap::MapSize{w, h};
        s.cellKnown = true;
    }
    const float scale = std::clamp(float(k), 0.75f, 6.0f);
    if (float(k) != s.k || scale != s.scale || targetWidth != s.targetW || targetHeight != s.targetH) {
        s.k = float(k);
        s.scale = scale;
        s.targetW = targetWidth;
        s.targetH = targetHeight;
        s.dirty = true;
    }
    if (s.look == minimap::Look::Hidden) {  // Ctrl+Tab hid it: neither drawn nor taking clicks
        if (s.shown) unblock();
        s.shown = false;
        return;
    }
    // 3.0.11-beta.6: the hints - made again for another map, another reference read, other switches
    const auto index = reference::index();
    if (!s.hintsMade || s.hintsMap != s.map || index != s.index || show.npcs != s.show.npcs ||
        show.warps != s.show.warps || show.mobs != s.show.mobs) {
        if (index != s.index) s.follow.map.clear();  // 3.0.11-beta.9: the way made again over the new pack
        s.index = index;
        s.show = show;
        buildHints(s);
        s.dirty = true;
    }
    followFrame(s);
    int cx = 0, cy = 0, hover = -1;  // the mark under the game's cursor names itself on the line
    if (!s.marks.empty() && readGameCursor(cx, cy))
        hover = minimap::markAt(s.geometry, s.size, s.cell, kCells, s.marks, float(cx) * float(k) - float(s.geometry.x),
                                float(cy) * float(k) - float(s.geometry.y));
    if (hover != s.hover) {
        s.hover = hover;
        s.dirty = true;
    }
    if (s.dirty) render(s);
    s.shown = true;
    block(s, game);
}

bool key(unsigned vk) {
    State& s = state();
    if (!s.on || vk != VK_TAB || !(GetKeyState(VK_CONTROL) & 0x8000)) return false;
    s.look = minimap::next(s.look);
    s.dirty = true;
    static const char* const kNames[] = {"opaque", "see-through", "hidden"};
    LOG_INFO("minimap: Ctrl+Tab - the new minimap %s", kNames[int(s.look)]);
    return true;
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.picture.width <= 0 || s.picture.height <= 0 || s.k <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the answer card
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    drawPicture(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.geometry.x, s.geometry.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button) return false;
    float tx = 0, ty = 0;
    if (!toTarget(s, game, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), tx, ty) || !minimap::inside(s.geometry, tx, ty))
        return false;
    if (m == WM_LBUTTONDOWN && s.clicks++ == 0)  // the check of the prototype: the character did not walk
        LOG_INFO("minimap: a click on the new minimap kept from the game (the player at %d, %d on %s)", s.cell.x,
                 s.cell.y, s.map.c_str());
    // 3.0.11-beta.7: the line with the map's name - its card in «Довідник» (when it is on); 3.0.11-beta.9: the lines
    // under it - the way's: the target's card (its «зупинити ›»), a monster's: its card, «і ще N»: the map's
    const float listTop = float(s.geometry.y + s.geometry.size + s.geometry.textH);
    if (m == WM_LBUTTONDOWN && ty >= float(s.geometry.y + s.geometry.size) && ty < listTop)
        referencewin::openMap(s.map);
    if (m == WM_LBUTTONDOWN && ty >= listTop && s.geometry.lineH > 0) {
        const size_t i = size_t((ty - listTop) / float(s.geometry.lineH));
        if (i < s.routeLines.size()) {
            referencewin::openMap(s.follow.target);
        } else if (i - s.routeLines.size() < s.lines.size()) {
            const Line& l = s.lines[i - s.routeLines.size()];
            if (l.mob > 0) referencewin::openMob(l.mob);
            else referencewin::openMap(s.map);
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
    LOG_INFO("route: the way to %s %s followed on the minimap", target.c_str(), walk ? "on foot" : "by Kafra");
    followFrame(s);
    s.dirty = true;
}

void unfollow() {
    State& s = state();
    if (!s.follow.active) return;
    LOG_INFO("route: the way to %s no more followed", s.follow.target.c_str());
    s.follow = routefollow::Follow{};
    s.routeLines.clear();
    s.dirty = true;
}

bool following(std::string& target, bool& walk) {
    const State& s = state();
    if (!s.follow.active) return false;
    target = s.follow.target;
    walk = s.follow.walk;
    return true;
}

}  // namespace uagfx::win::minimapproto
