#include "win/homun_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/homun_card.hpp"
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

namespace uagfx::win::homuncardwin {
namespace {

namespace hc = uagfx::homuncard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the homunculus window (where the manager keeps it, its class's vtable, its SendMsg), a window's shown
// flag; the homunculus as the game keeps it (the server's 0x022e): its name (24 bytes), level, the stats (ATK MATK HIT
// CRIT DEF MDEF FLEE, ints), the attack delay, HP, its most, SP, its most, the experience, its most, fullness,
// closeness (the level -1 while nothing is known)
constexpr uint32_t kWindowMgr = 0x727ED8, kHomunAt = kWindowMgr + 0x1E4, kHomunClass = 0x6981A0;
constexpr uint32_t kHomunSend = 0x467210, kShown = 0x38, kSendMsgSlot = 0x94;
constexpr uint32_t kName = 0x772E38, kStats = 0x772E58, kDelay = 0x772E74, kKnown = 0x772E78, kLevel = 0x772E7C;
constexpr uint32_t kHp = 0x772E80, kMaxHp = 0x772E84, kSp = 0x772E88, kMaxSp = 0x772E8C, kExp = 0x772E90;
constexpr uint32_t kMaxExp = 0x772E94, kHunger = 0x772E98, kIntimacy = 0x772EA0;
constexpr int kButtonMsg = 6;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hidden = 0;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    hc::Info info;
    hc::Show show;
    hc::Layout layout;
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

uint32_t gameHomun() {
    const uint32_t w = read32(kHomunAt);
    return w && read32(w) == kHomunClass && read32(read32(w) + kSendMsgSlot) == kHomunSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("homun: the game's homunculus window hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

hc::Info infoOf(const State& s) {
    hc::Info i;
    char name[25] = {};
    readGame(kName, name, 24);
    i.name = utf8Of(widened(name, s.codePage));
    i.level = int32_t(read32(kLevel));
    int32_t stats[7] = {};
    if (readGame(kStats, stats, sizeof stats))
        for (int k = 0; k < 7; ++k) i.stats[k] = stats[k];
    i.aspd = int32_t(read32(kDelay));
    i.hp = int32_t(read32(kHp));
    i.maxHp = int32_t(read32(kMaxHp));
    i.sp = int32_t(read32(kSp));
    i.maxSp = int32_t(read32(kMaxSp));
    i.exp = int32_t(read32(kExp));
    i.maxExp = int32_t(read32(kMaxExp));
    i.hunger = int32_t(read32(kHunger));
    i.intimacy = int32_t(read32(kIntimacy));
    return i;
}

hc::Texts texts() {
    hc::Texts t;
    t.title = say(settings::Text::HomunTitle);
    t.level = say(settings::Text::PetLevel);
    t.hunger = say(settings::Text::PetHunger);
    t.intimacy = say(settings::Text::PetIntimacy);
    const settings::Text close[8] = {settings::Text::HomunClose0, settings::Text::HomunClose1,
                                     settings::Text::HomunClose2, settings::Text::PetClose1,
                                     settings::Text::PetClose2,   settings::Text::PetClose3,
                                     settings::Text::PetClose4,   settings::Text::HomunClose7};
    for (int i = 0; i < 8; ++i) t.intimacyStates[i] = say(close[i]);
    t.feed = say(settings::Text::PetFeed);
    t.skills = say(settings::Text::HomunSkills);
    t.remove = say(settings::Text::HomunDelete);
    return t;
}

std::vector<Words> gdiWords(const std::vector<hc::Words>& in) {
    std::vector<Words> out;
    for (const hc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == hc::Align::Right    ? DT_RIGHT
                                 : w.align == hc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the homunculus", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    hc::paint(s.picture, s.layout, s.info, s.show);
    drawWords(s.picture, gdiWords(hc::words(s.layout, s.info, s.show, texts())));
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
    uapad::gamewindow::setBlocked(Blocker::Homun, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Homun, nullptr, nullptr, false);
    s.shown = false;
}

void press(State& s, hc::Button b) {
    const uint32_t window = gameHomun();
    if (!window) return;
    const char* const names[4] = {"feed", "its skills", "delete (the game asks first)", "close"};
    if (s.told++ < 30) LOG_INFO("homun: %s - the window's 0x%X", names[int(b)], unsigned(hc::gameButton(b)));
    reinterpret_cast<SendMsgFn>(uintptr_t(kHomunSend))(window, 0, kButtonMsg, hc::gameButton(b), 0, 0);
    s.key.clear();
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("homun: [interface] homun = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameHomun() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || int32_t(read32(kKnown)) == -1 || !game || k <= 0 || targetWidth <= 0 ||
        targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) {
        s.show = hc::Show{};
        LOG_INFO("homun: the game's homunculus window - the new card shows it");
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.info = infoOf(s);
    s.layout = hc::layout(targetWidth, targetHeight, s.scale);
    s.show.hover = -1;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const hc::Hit h = hc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == hc::Part::Button) s.show.hover = int(h.button);
    }
    const hc::Info& i = s.info;
    std::string key = i.name + ';' + std::to_string(i.level) + ',' + std::to_string(i.hp) + '/' +
                      std::to_string(i.maxHp) + ',' + std::to_string(i.sp) + '/' + std::to_string(i.maxSp) + ',' +
                      std::to_string(i.exp) + '/' + std::to_string(i.maxExp) + ',' + std::to_string(i.hunger) + ',' +
                      std::to_string(i.intimacy) + ',' + std::to_string(i.aspd) + ';' + std::to_string(s.show.hover) +
                      ',' + std::to_string(s.show.focus) + ';' + std::to_string(s.layout.x) + ',' +
                      std::to_string(s.layout.y) + '#' + std::to_string(uistyle::generation());
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
    const hc::Hit h = hc::hitTest(s.layout, lx, ly);
    if (h.part == hc::Part::Button) press(s, h.button);
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    press(s, hc::Button::Close);
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameHomun(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    int& f = s.show.focus;  // feed or its skills
    if (f != int(hc::Button::Feed) && f != int(hc::Button::Skills)) f = int(hc::Button::Feed);
    switch (c) {
        case PadCommand::Left: f = int(hc::Button::Feed); break;
        case PadCommand::Right: f = int(hc::Button::Skills); break;
        case PadCommand::Press: press(s, hc::Button(f)); break;
        case PadCommand::Close: press(s, hc::Button::Close); break;
    }
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

}  // namespace uagfx::win::homuncardwin
