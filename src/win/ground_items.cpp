#include "win/ground_items.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/game_camera.hpp"
#include "core/ground_items.hpp"
#include "core/ground_way.hpp"
#include "core/hotbar_panel.hpp"
#include "core/ui_style.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/reference_load.hpp"

namespace uagfx::win::groundwin {
namespace {

namespace gi = grounditems;
constexpr uint32_t kGround = 0x0B0F16, kText = 0xEEF2F8, kCardRgb = 0xD3A6FF, kGearRgb = 0x8FC1FF, kDearRgb = 0xF2CF7A;
constexpr size_t kMost = 48;  // labels at most at once

struct Label {
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int x = 0, y = 0;
};

struct State {
    gi::Ground ground;
    std::string map;
    std::vector<Label> pool;
    size_t used = 0;
    float k = 0;
    int told = 0;
    bool altTold = false;
};

State& state() {
    static State s;
    return s;
}

uint32_t rgbOf(gi::Worth w) {
    return w == gi::Worth::Card ? kCardRgb : w == gi::Worth::Gear ? kGearRgb : w == gi::Worth::Dear ? kDearRgb : kText;
}

std::string nameOf(int item, bool identified, unsigned codePage, const refindex::Index* ix) {
    if (identified) {
        const auto& names = clienttables::itemNames();
        const auto n = names.find(std::to_string(item));
        if (n != names.end()) return utf8Of(widened(hotbar::spaced(n->second), codePage));
    }
    if (ix) {
        const auto it = ix->items.find(item);
        if (it != ix->items.end()) return it->second.name;
    }
    return "#" + std::to_string(item);
}

}  // namespace

void packet(uint16_t id, const uint8_t* packet) {
    if (id == 0x009d || id == 0x009e || id == 0x00a1) state().ground.packet(id, packet);
}

void frame(HWND game, bool names, bool highlight, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.used = 0;
    if ((!names && !highlight) || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld()) return;
    const std::string map = readGameMap();
    if (map != s.map) {
        s.map = map;
        s.ground.clear();
    }
    if (s.ground.items().empty()) return;
    const bool alt = names && (GetAsyncKeyState(VK_MENU) & 0x8000) && GetForegroundWindow() == game;
    if (alt && !s.altTold) {
        s.altTold = true;
        LOG_INFO("ground items: Alt held - %zu item(s) on the ground named", s.ground.items().size());
    }
    if (!alt && !highlight) return;
    float view[12], at[3];
    int w = 0, h = 0, cellSize = 0;
    if (!readGameCamera(view) || !readPlayerWorld(at, w, h, cellSize) || cellSize <= 0) return;
    const int gameW = int(std::lround(targetWidth / k)), gameH = int(std::lround(targetHeight / k));
    const camera::Projection p = camera::projectionFor(gameW, gameH);
    const auto index = reference::index();
    const float scale = uistyle::readingScale(k);
    s.k = float(k);
    for (const auto& [id, it] : s.ground.items()) {
        if (s.used >= kMost) break;
        int type = -1, price = 0;
        if (index) {
            const auto r = index->items.find(it.item);
            if (r != index->items.end()) type = r->second.type, price = r->second.buy / 2;
        }
        const gi::Worth worth = gi::worth(type, price);
        if (!alt && worth == gi::Worth::Plain) continue;
        float wx = 0, wz = 0;
        uagfx::groundway::cellToWorld(it.x, it.y, w, h, cellSize, wx, wz);
        const float wy = at[1];
        const float vx = wx * view[0] + wy * view[3] + wz * view[6] + view[9];
        const float vy = wx * view[1] + wy * view[4] + wz * view[7] + view[10];
        const float vz = wx * view[2] + wy * view[5] + wz * view[8] + view[11];
        if (vz <= p.nearZ) continue;
        const float sx = vx / vz * p.f0 + p.cx, sy = vy / vz * p.f1 + p.cy;
        if (sx < 0 || sy < 0 || sx > float(gameW) || sy > float(gameH)) continue;
        const std::string text = gi::label(nameOf(it.item, it.identified, codePage, index.get()), it.amount);
        if (s.used >= s.pool.size()) s.pool.emplace_back();
        Label& l = s.pool[s.used++];
        const std::string key = text + '#' + std::to_string(int(worth)) + '#' + std::to_string(scale);
        if (key != l.key) {
            l.key = key;
            const float px = 11 * scale;
            const int lw = int(std::ceil(panel::wordsWidth(text, px, true) + 12 * scale));
            const int lh = int(std::ceil(17 * scale));
            if (panel::blank(l.picture, lw, lh)) {
                panel::roundRect(l.picture, panel::Box{0, 0, float(lw), float(lh)}, 5 * scale, kGround, 0.72f,
                                 rgbOf(worth), worth == gi::Worth::Plain ? 0.0f : 0.9f, 1.0f * scale);
                drawWords(l.picture, {Words{panel::Box{0, 0, float(lw), float(lh)}, utf8Wide(text), rgbOf(worth), px,
                                            true, DT_CENTER, false}});
                l.upload = true;
            }
        }
        l.x = int(sx * float(k)) - l.picture.width / 2;
        l.y = int(sy * float(k)) - l.picture.height - int(4 * scale);
    }
    if (s.used && s.told < 5) {
        ++s.told;
        LOG_INFO("ground items: %zu named on %s (%s)", s.used, map.c_str(), alt ? "Alt held" : "the valuable ones");
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.used || s.k <= 0) return;
    const size_t world = record.firstInterface(), before = record.commands().size();
    for (size_t i = 0; i < s.used; ++i) {
        Label& l = s.pool[i];
        if (l.picture.width > 0)
            drawPart(record, renderer, l.picture, l.texture, l.texW, l.texH, l.upload, l.x, l.y, s.k);
    }
    const size_t added = record.commands().size() - before;
    if (added) record.moveLastBefore(added, world);
}

}  // namespace uagfx::win::groundwin
