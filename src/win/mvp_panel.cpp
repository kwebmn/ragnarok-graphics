#include "win/mvp_panel.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <variant>
#include <vector>

#include "core/hud_layout.hpp"
#include "core/layout.hpp"
#include "core/mvp_timers.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/char_diary.hpp"
#include "win/game_camera.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/target_card.hpp"
#include "win/ui_cost.hpp"

namespace uagfx::win::mvpwin {
namespace {

using uapad::gamewindow::Blocker;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kText = 0xEEF2F8, kMuted = 0x9AA3B5, kGold = 0xE6CB8A;
constexpr uint32_t kBack = 0x8FD19E;
constexpr float kW = 280, kLineH = 19, kPad = 9, kTitleH = 22;
constexpr int kRows = 6, kLeastRespawnMs = 600000;

struct State {
    bool on = false, loaded = false, shown = false, upload = false;
    mvptimers::Timers timers;
    std::vector<mvptimers::Timer> rows;
    float k = 0;
    int targetW = 0, targetH = 0, x = 0, y = 0, w = 0, h = 0, hover = -1;
    float scale = 1;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(language(), t); }
std::string path() { return layout::join(gameDir(), layout::kMvpTimers); }

void load(State& s) {
    if (s.loaded) return;
    s.loaded = true;
    std::string text;
    if (readTextFile(path(), text)) s.timers = mvptimers::Timers::parse(text);
    s.timers.prune(chardiarywin::nowUnix());
}

void save(State& s) {
    if (!writeFileAtomic(path(), s.timers.text()) && s.told++ < 5)
        LOG_INFO("mvp: %s cannot be written", path().c_str());
}

std::string mapNow() {
    std::string map = readGameMap();
    const size_t dot = map.rfind('.');
    if (dot != std::string::npos) map.resize(dot);
    return map;
}

std::string mobName(int mob) {
    const auto index = reference::index();
    if (!index) return "#" + std::to_string(mob);
    const auto m = index->mobs.find(mob);
    return m == index->mobs.end() ? "#" + std::to_string(mob) : m->second.name;
}

}  // namespace

void packet(const packets::Event& e) {
    State& s = state();
    const auto* v = std::get_if<packets::Vanish>(&e);
    if (!s.on || !v || v->type != 1) return;
    const int cls = targetpanel::actorClass(v->id);
    if (cls <= 0) return;
    const auto index = reference::index();
    if (!index) return;
    const auto m = index->mobs.find(cls);
    if (m == index->mobs.end() || !m->second.boss) return;
    const std::string map = mapNow();
    int respawn = 0;
    const auto where = index->spawnsOf.find(cls);
    if (where != index->spawnsOf.end())
        for (const refindex::Where& w : where->second)
            if (w.map == map) respawn = w.respawnMs;
    if (respawn < kLeastRespawnMs) return;
    load(s);
    if (s.timers.died(cls, map, chardiarywin::nowUnix(), respawn)) {
        save(s);
        s.key.clear();
        LOG_INFO("mvp: %s died on %s - back in %d min", m->second.name.c_str(), map.c_str(), respawn / 60000);
    }
}

void frame(HWND game, bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("mvp: [interface] mvp_timers = %d", on ? 1 : 0);
    }
    const auto hide = [&] {
        if (s.shown) uapad::gamewindow::setBlocked(Blocker::Mvp, nullptr, nullptr, false);
        s.shown = false;
    };
    if (!on || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld()) return hide();
    load(s);
    const long long now = chardiarywin::nowUnix();
    s.rows = s.timers.shown(now);
    if (s.rows.size() > size_t(kRows)) s.rows.resize(kRows);
    if (s.rows.empty()) return hide();
    const float scale = uistyle::partScale(k) * hudedit::sizeOf(hudlayout::Part::Mvp);
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.w = int(std::lround(kW * scale));
    s.h = int(std::lround((2 * kPad + kTitleH + float(s.rows.size()) * kLineH) * scale));
    s.x = targetWidth - s.w - int(std::lround(12 * scale));
    s.y = int(targetHeight * 0.30f);
    hudedit::place(hudlayout::Part::Mvp, float(k), targetWidth, targetHeight, s.w, s.h, s.x, s.y);
    s.hover = -1;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const float lx = float(cx) * float(k) - float(s.x), ly = float(cy) * float(k) - float(s.y);
        const float top = (kPad + kTitleH) * scale;
        if (lx >= 0 && lx < float(s.w) && ly >= top) {
            const int row = int((ly - top) / (kLineH * scale));
            if (row < int(s.rows.size())) s.hover = row;
        }
    }
    std::string key = std::to_string(s.w) + 'x' + std::to_string(s.h) + 'h' + std::to_string(s.hover) + '#' +
                      std::to_string(uistyle::generation());
    std::vector<std::pair<std::string, std::string>> lines;
    for (const mvptimers::Timer& t : s.rows) {
        lines.emplace_back(mobName(t.mob) + " \xC2\xB7 " + t.map, mvptimers::left(t.back(), now));
        key += '|' + lines.back().first + lines.back().second;
    }
    if (key != s.key) {
        s.key = key;
        const uicost::Paint cost("the MVP panel", s.w, s.h);
        if (panel::blank(s.picture, s.w, s.h)) {
            const float sc = scale;
            panel::roundRect(s.picture, panel::Box{0, 0, float(s.w), float(s.h)}, 10 * sc, kCard, uistyle::panel(),
                             kWhite, 0.09f, 1.0f * sc);
            std::vector<Words> out;
            out.push_back(Words{panel::Box{kPad * sc, kPad * sc, float(s.w) - 2 * kPad * sc, (kTitleH - 4) * sc},
                                utf8Wide(say(settings::Text::MvpTitle)), kGold, 13 * sc, true, DT_LEFT, false});
            for (size_t i = 0; i < lines.size(); ++i) {
                const float y = (kPad + kTitleH + float(i) * kLineH) * sc;
                if (int(i) == s.hover)
                    panel::roundRect(s.picture, panel::Box{4 * sc, y, float(s.w) - 8 * sc, kLineH * sc}, 5 * sc,
                                     kWhite, 0.08f);
                const bool back = s.rows[i].back() <= now;
                out.push_back(Words{panel::Box{kPad * sc, y, float(s.w) - (2 * kPad + 64) * sc, kLineH * sc},
                                    utf8Wide(lines[i].first), int(i) == s.hover ? kText : kMuted, 12 * sc, false,
                                    DT_LEFT, false});
                out.push_back(Words{panel::Box{float(s.w) - (kPad + 64) * sc, y, 64 * sc, kLineH * sc},
                                    utf8Wide(lines[i].second), back ? kBack : kText, 12 * sc, true, DT_RIGHT, false});
            }
            drawWords(s.picture, out);
            s.upload = true;
        }
    }
    s.shown = true;
    RECT client{};
    if (!GetClientRect(game, &client)) return;
    const RECT r{LONG(int64_t(s.x) * client.right / targetWidth), LONG(int64_t(s.y) * client.bottom / targetHeight),
                 LONG(int64_t(s.x + s.w) * client.right / targetWidth + 1),
                 LONG(int64_t(s.y + s.h) * client.bottom / targetHeight + 1)};
    uapad::gamewindow::setBlocked(Blocker::Mvp, game, &r, false);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.x, s.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    (void)wp;
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.x), ly = ty - float(s.y);
    if (lx < 0 || ly < 0 || lx >= float(s.w) || ly >= float(s.h)) return false;
    result = 0;
    const float top = (kPad + kTitleH) * s.scale;
    if (m == WM_LBUTTONDOWN && ly >= top) {
        const int row = int((ly - top) / (kLineH * s.scale));
        if (row >= 0 && row < int(s.rows.size())) {
            LOG_INFO("mvp: %s on %s forgotten", mobName(s.rows[row].mob).c_str(), s.rows[row].map.c_str());
            s.timers.remove(s.rows[row].mob, s.rows[row].map);
            save(s);
            s.key.clear();
        }
    }
    return true;
}

}  // namespace uagfx::win::mvpwin
