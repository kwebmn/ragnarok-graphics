#include "win/mvp_panel.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
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
#include "win/uaui.hpp"
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
    bool uiShown = false;  // 3.0.18-beta.25: drawn by uaUI
    std::string viewKey;
    std::vector<std::pair<std::string, std::string>> lines;
    std::vector<bool> back;
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

// 3.0.18-beta.25 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 4 - by hand): the MVP panel in RmlUi's
// markup - its card, the title, each MVP's name · map and its time left (green when back), the row pointed at lit;
// drawn only (passive): the clicks and the blocker stay this part's; sizes by its own scale
const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="mp" data-model="mvp" data-style-width="w + 'px'" data-style-height="h + 'px'"
  data-style-border-radius="10 * s + 'px'" data-style-border-width="s + 'px'" data-style-background-color="ground">
<span class="title" data-style-left="9 * s + 'px'" data-style-top="9 * s + 'px'" data-style-width="w - 18 * s + 'px'"
  data-style-height="18 * s + 'px'" data-style-line-height="18 * s + 'px'" data-style-font-size="13 * s + 'px'"
  >{{title}}</span>
<div data-for="r, i : rows">
 <div class="lit" data-if="i == hover" data-style-left="4 * s + 'px'" data-style-top="(31 + 19 * i) * s + 'px'"
   data-style-width="w - 8 * s + 'px'" data-style-height="19 * s + 'px'" data-style-border-radius="5 * s + 'px'"></div>
 <span data-attr-class="i == hover ? 'name hot' : 'name'" data-style-left="9 * s + 'px'"
   data-style-top="(31 + 19 * i) * s + 'px'" data-style-width="w - 82 * s + 'px'" data-style-height="19 * s + 'px'"
   data-style-line-height="19 * s + 'px'" data-style-font-size="12 * s + 'px'">{{r.name}}</span>
 <span data-attr-class="r.back ? 'left back' : 'left'" data-style-left="w - 73 * s + 'px'"
   data-style-top="(31 + 19 * i) * s + 'px'" data-style-width="64 * s + 'px'" data-style-height="19 * s + 'px'"
   data-style-line-height="19 * s + 'px'" data-style-font-size="12 * s + 'px'">{{r.left}}</span>
</div>
</body></rml>)RML";

struct RowV {
    Rml::String name, left;
    bool back = false;
};

struct Ui {
    uaui::Doc doc;
    float w = 0, h = 0, s = 1;
    int hover = -1;
    Rml::String title, ground;
    std::vector<RowV> rows;
    std::string key;
};

Ui& ui() {
    static Ui u;
    return u;
}

bool uiFrame(State& s) {
    Ui& u = ui();
    if (!uaui::ready() || !s.shown || s.lines.empty()) {
        u.doc.show(false);
        return false;
    }
    if (!u.doc.made()) {
        u.doc.passive = true;
        if (!u.doc.make("mvp", kDoc, [&](Rml::DataModelConstructor& m) {
                if (auto r = m.RegisterStruct<RowV>()) {
                    r.RegisterMember("name", &RowV::name);
                    r.RegisterMember("left", &RowV::left);
                    r.RegisterMember("back", &RowV::back);
                }
                m.RegisterArray<std::vector<RowV>>();
                m.Bind("w", &u.w);
                m.Bind("h", &u.h);
                m.Bind("s", &u.s);
                m.Bind("hover", &u.hover);
                m.Bind("title", &u.title);
                m.Bind("ground", &u.ground);
                m.Bind("rows", &u.rows);
            }))
            return false;
    }
    if (s.viewKey != u.key) {
        u.key = s.viewKey;
        u.w = float(s.w), u.h = float(s.h), u.s = s.scale, u.hover = s.hover;
        char g[16];
        std::snprintf(g, sizeof g, "#%06x%02x", unsigned(uaui::themed(0x10141C)),
                      int(std::lround(uistyle::panel() * 255)) & 0xFF);
        u.ground = g;
        u.title = say(settings::Text::MvpTitle);
        u.rows.clear();
        for (size_t i = 0; i < s.lines.size(); ++i)
            u.rows.push_back(RowV{s.lines[i].first, s.lines[i].second, i < s.back.size() && s.back[i]});
        u.doc.dirtyAll();
    }
    u.doc.place(s.x, s.y);
    u.doc.show(true);
    return true;
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
        s.uiShown = uiFrame(s);
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
    s.lines = lines;
    s.back.clear();
    for (const mvptimers::Timer& t : s.rows) s.back.push_back(t.back() <= now);
    s.viewKey = key;
    if (key != s.key && !uaui::ready()) {  // 3.0.18-beta.25: uaUI's panel needs no picture
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
    s.uiShown = uiFrame(s);
    RECT client{};
    if (!GetClientRect(game, &client)) return;
    const RECT r{LONG(int64_t(s.x) * client.right / targetWidth), LONG(int64_t(s.y) * client.bottom / targetHeight),
                 LONG(int64_t(s.x + s.w) * client.right / targetWidth + 1),
                 LONG(int64_t(s.y + s.h) * client.bottom / targetHeight + 1)};
    uapad::gamewindow::setBlocked(Blocker::Mvp, game, &r, false);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.uiShown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
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
