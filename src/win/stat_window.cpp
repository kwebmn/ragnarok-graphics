#include "win/stat_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "core/char_status.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/stat_window.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::statswin {
namespace {

namespace sw = uagfx::statwin;

// uaRO.exe's: the windows' manager's «Характеристики» (UI window 0xB, its class's vtable, its SendMsg FUN_004722a0 -
// message 6 with 0x87 its close button), a window's shown flag; the game mode's SendMsg (FUN_005707d0, vtable + 0x14)
// and its message 0x20 a stat's point (13 STR .. 18 LUK, 1)
constexpr uint32_t kWindowMgr = 0x727ED8, kStatsAt = kWindowMgr + 0x80, kStatsClass = 0x698700;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kStatsSendMsg = 0x4722A0;
constexpr int kButtonMsg = 6, kCloseButton = 0x87;
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0;
constexpr int kRaiseMsg = 0x20, kFirstStat = 13;
constexpr ULONGLONG kReadMs = 200;

using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);

struct State {
    WindowShadow shadow;  // 3.0.11-beta.49
    bool on = false, shown = false, hid = false, stateRead = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    float fx = 0.57f, fy = 0.2f;  // its place: a share of the free room to its left and above it (the concept's)
    sw::Data data;
    ULONGLONG readAt = 0;
    sw::Show show;
    sw::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    bool moving = false;
    float grabX = 0, grabY = 0;
    int told = 0, pressesTold = 0;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(startup().lang, t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

uint32_t gameWindow() {
    const uint32_t w = read32(kStatsAt);
    return w && read32(w) == kStatsClass ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.hid = false;
        return;
    }
    if (on) {
        if (read32(window + kShown) && write32(window + kShown, 0) && !s.hid) {
            LOG_INFO("stats: the game's «Характеристики» window hidden for the new one (its shown flag 0)");
            s.hid = true;
        }
    } else if (s.hid) {
        write32(window + kShown, 1);
        s.hid = false;
        LOG_INFO("stats: the game's «Характеристики» window back");
    }
}

void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    float x = 0, y = 0;
    if (readTextFile(layout::join(gameDir(), layout::kStatsState), text) &&
        std::sscanf(text.c_str(), "at %f %f", &x, &y) == 2 && x >= 0 && x <= 1 && y >= 0 && y <= 1) {
        s.fx = x;
        s.fy = y;
    }
}

void keepState(const State& s) {
    char text[48];
    std::snprintf(text, sizeof text, "at %.4f %.4f\n", s.fx, s.fy);
    const std::string path = layout::join(gameDir(), layout::kStatsState);
    if (!writeFileAtomic(path, std::string(text))) LOG_INFO("stats: %s cannot be written", path.c_str());
}

void readData(State& s) {
    charstatus::Status st;
    if (!readOwnStatus(st)) return;
    sw::Data& d = s.data;
    for (int i = 0; i < sw::kStats; ++i) {
        d.base[i] = st.statBase[i];
        d.bonus[i] = st.statBonus[i];
        d.need[i] = st.statNeed[i];
    }
    d.free = st.statusPoints;
    d.atk = st.atk;
    d.atk2 = st.atk2;
    d.matkMin = st.matkMin;
    d.matkMax = st.matkMax;
    d.hit = st.hit;
    d.crit = st.crit;
    d.def = st.def;
    d.def2 = st.def2;
    d.mdef = st.mdef;
    d.mdef2 = st.mdef2;
    d.flee = st.flee;
    d.flee2 = st.flee2;
    d.aspd = st.aspd;
}

sw::Texts texts() {
    sw::Texts t;
    t.title = say(settings::Text::StatsTitle);
    t.hotkey = "Alt+A";
    t.free = say(settings::Text::StatsFree);
    const settings::Text cols[5] = {settings::Text::StatsColumnStat, settings::Text::StatsColumnBase,
                                    settings::Text::StatsColumnBonus, settings::Text::StatsColumnTotal,
                                    settings::Text::StatsColumnCost};
    for (int i = 0; i < 5; ++i) t.columns[i] = say(cols[i]);
    t.battle = say(settings::Text::StatsBattle);
    t.cast = say(settings::Text::StatsCast);
    t.maxed = say(settings::Text::StatsMaxed);
    t.cost = say(settings::Text::StatsCost);
    t.shortBy = say(settings::Text::StatsShort);
    t.top = say(settings::Text::StatsTop);
    const settings::Text effects[sw::kStats] = {settings::Text::StatsEffectStr, settings::Text::StatsEffectAgi,
                                                settings::Text::StatsEffectVit, settings::Text::StatsEffectInt,
                                                settings::Text::StatsEffectDex, settings::Text::StatsEffectLuk};
    for (int i = 0; i < sw::kStats; ++i) t.effects[i] = say(effects[i]);
    t.hint = say(settings::Text::StatsHint);
    t.decimal = startup().lang == settings::Lang::En ? '.' : ',';
    return t;
}

float measure(const std::string& utf8Text, float px, bool bold) {
    return float(wordsWidth(utf8Wide(utf8Text), px, bold));
}

std::vector<Words> gdiWords(const std::vector<sw::Words>& in) {
    std::vector<Words> out;
    for (const sw::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == sw::Align::Right    ? DT_RIGHT
                                 : w.align == sw::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("«Характеристики»", s.layout.w, s.layout.h);  // 3.0.11-beta.55
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;  // 3.0.11-beta.57: in the memory it had
    sw::paint(s.picture, s.layout, s.data, s.show);
    drawWords(s.picture, gdiWords(sw::words(s.layout, s.data, s.show, texts(), measure)));
    s.upload = true;
}

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own, not the world's
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Stats, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Stats, nullptr, nullptr, false);
    s.shown = false;
    s.moving = false;
}

bool raise(int stat) {
    const uint32_t mode = read32(kModeNow);
    if (!mode || !inGameWorld() || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, kRaiseMsg, kFirstStat + stat, 1, 0);
    return true;
}

void closeGameWindow() {
    const uint32_t window = gameWindow();
    if (window && read32(read32(window) + kSendMsgSlot) == kStatsSendMsg)
        reinterpret_cast<SendMsgFn>(uintptr_t(kStatsSendMsg))(window, 0, kButtonMsg, kCloseButton, 0, 0);
}

}  // namespace

void frame(HWND game, bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("stats: [interface] status = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameWindow() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    readState(s);
    const float scale = uistyle::readingScale(k);  // 3.0.11-beta.67: [interface] text_size's too
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const ULONGLONG now = GetTickCount64();
    if (now - s.readAt >= kReadMs) {
        s.readAt = now;
        readData(s);
    }
    const sw::Texts t = texts();
    s.layout = sw::layout(0, 0, scale, s.data, t, measure);
    s.layout.x = int(std::lround(s.fx * float(std::max(0, targetWidth - s.layout.w))));
    s.layout.y = int(std::lround(s.fy * float(std::max(0, targetHeight - s.layout.h))));
    s.show.hoverRow = s.show.hoverPlus = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy) && !s.moving) {
        const sw::Hit h = sw::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == sw::Part::Row) s.show.hoverRow = h.index;
        if (h.part == sw::Part::Plus) s.show.hoverPlus = h.index;
        s.show.hoverClose = h.part == sw::Part::Close;
    }
    const sw::Data& d = s.data;
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' + std::to_string(scale) +
                      ';' + std::to_string(s.show.hoverRow) + ',' + std::to_string(s.show.hoverPlus) + ',' +
                      std::to_string(s.show.selected) + (s.show.hoverClose ? "x" : "") + ';' + std::to_string(d.free) +
                      ';';
    for (int i = 0; i < sw::kStats; ++i)
        key += std::to_string(d.base[i]) + '+' + std::to_string(d.bonus[i]) + '/' + std::to_string(d.need[i]) + ',';
    const int battle[] = {d.atk, d.atk2, d.matkMin, d.matkMax, d.hit, d.crit, d.def, d.def2, d.mdef, d.mdef2, d.flee,
                          d.flee2, d.aspd};
    for (int v : battle) key += std::to_string(v) + ',';
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {  // its grounds
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the minimap
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;  // 3.0.11-beta.49: its shadow under it
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                  s.scale, s.k);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool shown() { return state().shown; }

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    (void)wp;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL && m != WM_MOUSEMOVE) return false;
    float tx = 0, ty = 0;
    bool fromGame = false;  // 3.0.11-beta.43: where the game's cursor stands, as the hover
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, &fromGame)) return false;
    if (s.moving && (m == WM_MOUSEMOVE || m == WM_LBUTTONUP)) {  // its title dragged
        const int roomX = std::max(1, s.targetW - s.layout.w), roomY = std::max(1, s.targetH - s.layout.h);
        s.fx = std::clamp((tx - s.grabX) / float(roomX), 0.0f, 1.0f);
        s.fy = std::clamp((ty - s.grabY) / float(roomY), 0.0f, 1.0f);
        if (m == WM_LBUTTONUP) {
            s.moving = false;
            keepState(s);
        }
        result = 0;
        return true;
    }
    const sw::Hit h = sw::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && s.pressesTold < 8) {  // where the presses land
        ++s.pressesTold;
        int cx = -1, cy = -1;
        readGameCursor(cx, cy);
        LOG_INFO("stats: a press at the window's (%d, %d), the game's cursor (%d, %d) - the target's (%.0f, %.0f) by "
                 "the %s; «Характеристики» at %d, %d %dx%d: part %d", GET_X_LPARAM(lp), GET_Y_LPARAM(lp), cx, cy, tx,
                 ty, fromGame ? "game's cursor" : "message's point", s.layout.x, s.layout.y, s.layout.w, s.layout.h,
                 int(h.part));
    }
    if (h.part == sw::Part::None || m == WM_MOUSEMOVE) return false;
    result = 0;
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    switch (h.part) {
        case sw::Part::Close:
            if (s.told++ < 12) LOG_INFO("stats: its close - the game's own window asked to close (its 0x87)");
            closeGameWindow();
            break;
        case sw::Part::Plus:
            s.show.selected = h.index;
            if (sw::canRaise(s.data, h.index)) {
                const bool sent = raise(h.index);
                s.readAt = 0;
                if (s.told++ < 12)
                    LOG_INFO("stats: %s +1 %s", sw::kNames[h.index],
                             sent ? "asked as the game's own «+»" : "not asked");
            }
            break;
        case sw::Part::Row:
            s.show.selected = h.index;
            break;
        case sw::Part::Title:
            s.moving = true;
            s.grabX = tx - float(s.layout.x);
            s.grabY = ty - float(s.layout.y);
            break;
        default:
            break;
    }
    s.key.clear();
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (!s.on || !inGameWorld()) return;
    if (const uint32_t window = gameWindow()) hold(s, window, true);
}

}  // namespace uagfx::win::statswin
