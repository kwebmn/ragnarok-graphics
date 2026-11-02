#include "win/merc_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "core/merc_card.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::merccardwin {
namespace {

namespace mc = uagfx::merccard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the mercenary window (where the manager keeps it, its class's vtable, its SendMsg), a window's shown
// flag; the mercenary as the game keeps it: its name (24 bytes), the stats (ATK MATK HIT CRIT DEF MDEF FLEE, ints), the
// attack delay, its class (-1 while nothing is known), level, HP, its most, SP, its most, loyalty, summons, kills, the
// contract's end (a 32-bit time_t)
constexpr uint32_t kWindowMgr = 0x727ED8, kMercAt = kWindowMgr + 0x1EC, kMercClass = 0x6982D0;
constexpr uint32_t kMercSend = 0x468100, kShown = 0x38, kSendMsgSlot = 0x94;
constexpr uint32_t kName = 0x772EE0, kStats = 0x772F00, kDelay = 0x772F1C, kClass = 0x772F20, kLevel = 0x772F24;
constexpr uint32_t kHp = 0x772F28, kMaxHp = 0x772F2C, kSp = 0x772F30, kMaxSp = 0x772F34, kLoyalty = 0x772F38;
constexpr uint32_t kSummons = 0x772F3C, kKills = 0x772F40, kEnds = 0x772F48;
constexpr int kButtonMsg = 6;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hidden = 0;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    mc::Info info;
    mc::Show show;
    mc::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
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

uint32_t gameMerc() {
    const uint32_t w = read32(kMercAt);
    return w && read32(w) == kMercClass && read32(read32(w) + kSendMsgSlot) == kMercSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("merc: the game's mercenary window hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

mc::Info infoOf(const State& s) {
    mc::Info i;
    char name[25] = {};
    readGame(kName, name, 24);
    i.name = utf8Of(widened(name, s.codePage));
    i.kind = mc::kindOf(int32_t(read32(kClass)));
    i.level = int32_t(read32(kLevel));
    int32_t stats[7] = {};
    if (readGame(kStats, stats, sizeof stats))
        for (int k = 0; k < 7; ++k) i.stats[k] = stats[k];
    i.aspd = int32_t(read32(kDelay));
    i.hp = int32_t(read32(kHp));
    i.maxHp = int32_t(read32(kMaxHp));
    i.sp = int32_t(read32(kSp));
    i.maxSp = int32_t(read32(kMaxSp));
    i.loyalty = int32_t(read32(kLoyalty));
    i.summons = int32_t(read32(kSummons));
    i.kills = int32_t(read32(kKills));
    const time_t ends = time_t(int32_t(read32(kEnds)));
    char when[32] = {};
    const std::tm* lt = ends > 0 ? std::localtime(&ends) : nullptr;
    if (lt) std::strftime(when, sizeof when, "%d.%m %H:%M", lt);
    i.ends = when;
    return i;
}

mc::Texts texts() {
    mc::Texts t;
    t.kinds[0] = say(settings::Text::MercArcher);
    t.kinds[1] = say(settings::Text::MercSpearman);
    t.kinds[2] = say(settings::Text::MercSwordsman);
    t.level = say(settings::Text::PetLevel);
    t.facts[0] = say(settings::Text::MercEnds);
    t.facts[1] = say(settings::Text::MercLoyalty);
    t.facts[2] = say(settings::Text::MercSummons);
    t.facts[3] = say(settings::Text::MercKills);
    t.skills = say(settings::Text::HomunSkills);
    t.dismiss = say(settings::Text::MercDismiss);
    return t;
}

std::vector<Words> gdiWords(const std::vector<mc::Words>& in) {
    std::vector<Words> out;
    for (const mc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == mc::Align::Right    ? DT_RIGHT
                                 : w.align == mc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the mercenary", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    mc::paint(s.picture, s.layout, s.info, s.show);
    drawWords(s.picture, gdiWords(mc::words(s.layout, s.info, s.show, texts())));
    s.upload = true;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(Blocker::Merc, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Merc, nullptr, nullptr, false);
    s.shown = false;
}

void press(State& s, mc::Button b) {
    const uint32_t window = gameMerc();
    if (!window) return;
    const char* const names[3] = {"its skills", "dismiss (the game asks first)", "close"};
    if (s.told++ < 30) LOG_INFO("merc: %s - the window's 0x%X", names[int(b)], unsigned(mc::gameButton(b)));
    reinterpret_cast<SendMsgFn>(uintptr_t(kMercSend))(window, 0, kButtonMsg, mc::gameButton(b), 0, 0);
    s.key.clear();
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("merc: [interface] merc = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameMerc() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || int32_t(read32(kClass)) == -1 || !game || k <= 0 || targetWidth <= 0 ||
        targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) {
        s.show = mc::Show{};
        LOG_INFO("merc: the game's mercenary window - the new card shows it");
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.info = infoOf(s);
    s.layout = mc::layout(targetWidth, targetHeight, s.scale);
    s.show.hover = -1;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const mc::Hit h = mc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == mc::Part::Button) s.show.hover = int(h.button);
    }
    const mc::Info& i = s.info;
    std::string key = i.name + ';' + std::to_string(int(i.kind)) + ',' + std::to_string(i.level) + ',' +
                      std::to_string(i.hp) + '/' + std::to_string(i.maxHp) + ',' + std::to_string(i.sp) + '/' +
                      std::to_string(i.maxSp) + ',' + i.ends + ',' + std::to_string(i.loyalty) + ',' +
                      std::to_string(i.summons) + ',' + std::to_string(i.kills) + ',' + std::to_string(i.aspd) + ';' +
                      std::to_string(s.show.hover) + (s.show.focus ? "f" : "") + ';' + std::to_string(s.layout.x) +
                      ',' + std::to_string(s.layout.y) + '#' + std::to_string(uistyle::generation());
    for (int st : i.stats) key += ',' + std::to_string(st);
    if (key != s.key) {
        s.key = key;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                  s.scale, s.k);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
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
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    const mc::Hit h = mc::hitTest(s.layout, lx, ly);
    if (h.part == mc::Part::Button) press(s, h.button);
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    press(s, mc::Button::Close);
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameMerc(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    s.show.focus = true;
    if (c == PadCommand::Press) press(s, mc::Button::Skills);
    else press(s, mc::Button::Close);
    s.key.clear();
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::merccardwin
