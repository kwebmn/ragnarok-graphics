#include "win/minimap_proto.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "core/minimap_proto.hpp"
#include "core/pack_source.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"

namespace uagfx::win::minimapproto {
namespace {

constexpr int kCells = 100;  // the view: 100 cells across, as the minimap test's cards
const std::string kUi = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";  // 유저인터페이스

struct State {
    bool on = false, shown = false, dirty = true, upload = true;
    bool hidOld = false;      // the game's own window 14 deleted by us (the game makes it again on a new map)
    bool hideFailed = false;  // told once a map
    std::string map;
    std::vector<uint32_t> argb;  // the map's own minimap picture (the game's), the whole map
    int picW = 0, picH = 0;
    minimap::MapSize size;
    minimap::Cell cell;
    bool cellKnown = false;
    int clicks = 0;  // kept from the game on this map
    minimap::Geometry geometry;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0, targetW = 0, targetH = 0;
    float k = 0, scale = 1;
    std::unique_ptr<gamefiles::Reader> files;
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

void render(State& s) {
    s.geometry = minimap::geometry(s.targetW, s.targetH, s.scale);
    minimap::paint(s.picture, s.geometry, s.argb, s.picW, s.picH, s.size, s.cell, kCells);
    char text[96];
    if (s.cellKnown)
        std::snprintf(text, sizeof text, "%s  %d, %d", s.map.c_str(), s.cell.x, s.cell.y);
    else
        std::snprintf(text, sizeof text, "%s", s.map.c_str());
    const float sc = s.scale;
    drawWords(s.picture, {Words{panel::Box{4 * sc, float(s.geometry.size), float(s.geometry.size) - 8 * sc,
                                           float(s.geometry.textH)},
                                utf8Wide(text), panel::kText, 12.5f * sc, false, DT_CENTER}});
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
                 LONG(int64_t(g.y + g.size + g.textH) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Minimap, &r, false);
}

void unblock() { uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Minimap, nullptr, false); }

}  // namespace

void frame(HWND game, bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
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
        s.cellKnown = false;
        s.hideFailed = false;
        loadPicture(s);
        s.dirty = true;
    }
    // the game's own minimap, each time the game makes it again (a new map, Ctrl+Tab): hidden as Ctrl+Tab hides it
    if (gameMinimapShown()) {
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
    if (s.dirty) render(s);
    s.shown = true;
    block(s, game);
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
    result = 0;
    return true;
}

}  // namespace uagfx::win::minimapproto
