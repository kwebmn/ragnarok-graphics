#include "win/guild_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/char_block.hpp"
#include "core/guild_card.hpp"
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

namespace uagfx::win::guildcardwin {
namespace {

namespace gc = uagfx::guildcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the guild's six windows (their classes' vtables and SendMsgs, in the manager from + 0x170), a window's
// shown flag; the guild object: its name's, master's and territory's words (a string's pointer and count), its
// numbers (the level, the members of the most, the average level, the experience and the next, the tax points), its
// members' list (a node: next +0, the member +8 - its name's words +0xC / +0x10, job +0x20, title +0x30 / +0x34,
// level +0x3C, online +0x58)
constexpr uint32_t kWindowMgr = 0x727ED8, kShown = 0x38, kSendMsgSlot = 0x94;
constexpr uint32_t kClasses[gc::kWindows] = {0x69A988, 0x69AAC8, 0x69AB64, 0x69ACA4, 0x69AD40, 0x69ADDC};
constexpr uint32_t kSends[gc::kWindows] = {0x49D980, 0x4A1A50, 0x4A3630, 0x4A5920, 0x4A6AF0, 0x4A7580};
constexpr uint32_t kName = 0x74B654, kMaster = 0x74B664, kTerritory = 0x74B6A8, kLevel = 0x74B690;
constexpr uint32_t kMost = 0x74B69C, kAverage = 0x74B6A0, kExp = 0x74B6B4, kNextExp = 0x74B6B8, kTax = 0x74B6C0;
constexpr uint32_t kMembers = 0x74B70C, kMember = 8;
constexpr int kButtonMsg = 6;
constexpr size_t kMostMembers = 100;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hidden = 0;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    gc::Info info;
    std::vector<gc::Member> members;
    gc::Show show;
    gc::Layout layout;
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

std::string say(settings::Text t) { return settings::tr(language(), t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

// The guild's tab open now (its window), 0 none
uint32_t gameGuild() {
    for (int i = 0; i < gc::kWindows; ++i) {
        const uint32_t w = read32(kWindowMgr + gc::kFirstAt + 4 * uint32_t(i));
        if (w && read32(w) == kClasses[i] && read32(read32(w) + kSendMsgSlot) == kSends[i]) return w;
    }
    return 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("guild: the game's guild window hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

std::string text(const State& s, uint32_t ptr, uint32_t len) {
    if (!ptr || !len || len > 64) return "";
    char buf[65] = {};
    return readGame(ptr, buf, len) ? utf8Of(widened(std::string(buf, len), s.codePage)) : "";
}

void readGuild(State& s) {
    gc::Info& g = s.info;
    g.name = text(s, read32(kName), read32(kName + 4));
    g.master = text(s, read32(kMaster), read32(kMaster + 4));
    g.territory = text(s, read32(kTerritory), read32(kTerritory + 4));
    g.level = int32_t(read32(kLevel));
    g.most = int32_t(read32(kMost));
    g.average = int32_t(read32(kAverage));
    g.exp = int32_t(read32(kExp));
    g.nextExp = int32_t(read32(kNextExp));
    g.tax = int32_t(read32(kTax));
    s.members.clear();
    const uint32_t head = read32(kMembers);
    if (head) {
        uint32_t node = read32(head);
        for (size_t n = 0; node && node != head && n < kMostMembers; ++n, node = read32(node)) {
            uint8_t raw[0x5C] = {};
            if (!readGame(node + kMember, raw, sizeof raw)) break;
            uint32_t f[0x17];
            std::memcpy(f, raw, sizeof f);
            gc::Member m;
            m.name = text(s, f[3], f[4]);
            m.title = text(s, f[12], f[13]);
            const char* job = charblock::jobName(int(f[8]));
            m.job = job && *job ? job : "#" + std::to_string(f[8]);
            m.level = int(f[15]);
            m.online = f[22] != 0;
            s.members.push_back(std::move(m));
        }
    }
    g.members = int(s.members.size());
}

gc::Texts texts() {
    gc::Texts t;
    t.title = say(settings::Text::GuildTitle);
    t.tabs[0] = say(settings::Text::GuildInfo);
    t.tabs[1] = say(settings::Text::GuildMembers);
    const settings::Text facts[7] = {settings::Text::GuildMaster,  settings::Text::GuildLevel,
                                     settings::Text::GuildMembers, settings::Text::GuildAverage,
                                     settings::Text::GuildExp,     settings::Text::GuildTax,
                                     settings::Text::GuildTerritory};
    for (int i = 0; i < 7; ++i) t.facts[i] = say(facts[i]);
    t.none = say(settings::Text::GuildNone);
    t.online = say(settings::Text::GuildOnline);
    return t;
}

std::vector<Words> gdiWords(const std::vector<gc::Words>& in) {
    std::vector<Words> out;
    for (const gc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == gc::Align::Right    ? DT_RIGHT
                                 : w.align == gc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the guild", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    gc::paint(s.picture, s.layout, s.members, s.show);
    drawWords(s.picture, gdiWords(gc::words(s.layout, s.info, s.members, s.show, texts())));
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
    uapad::gamewindow::setBlocked(Blocker::Guild, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Guild, nullptr, nullptr, false);
    s.shown = false;
}

void closeIt(State& s) {
    const uint32_t window = gameGuild();
    if (!window) return;
    if (s.told++ < 30) LOG_INFO("guild: closed (the open tab's 0x87)");
    reinterpret_cast<SendMsgFn>(uintptr_t(read32(read32(window) + kSendMsgSlot)))(window, 0, kButtonMsg,
                                                                                   gc::kCloseButton, 0, 0);
    s.key.clear();
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("guild: [interface] guild = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameGuild() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) LOG_INFO("guild: the game's guild window - the new card shows it");
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    readGuild(s);
    s.layout = gc::layout(targetWidth, targetHeight, s.scale, s.show.tab, s.members.size());
    s.show.scroll = std::clamp(s.show.scroll, 0, gc::mostScroll(s.layout, s.members.size()));
    s.show.hoverTab = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const gc::Hit h = gc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == gc::Part::Tab) s.show.hoverTab = h.index;
        s.show.hoverClose = h.part == gc::Part::Close;
    }
    const gc::Info& g = s.info;
    std::string key = g.name + ';' + g.master + ';' + g.territory + ';' + std::to_string(g.level) + ',' +
                      std::to_string(g.most) + ',' + std::to_string(g.average) + ',' + std::to_string(g.exp) + ',' +
                      std::to_string(g.nextExp) + ',' + std::to_string(g.tax) + ';' + std::to_string(int(s.show.tab)) +
                      ',' + std::to_string(s.show.scroll) + ',' + std::to_string(s.show.hoverTab) +
                      (s.show.hoverClose ? "x" : "") + ';' + std::to_string(s.layout.x) + ',' +
                      std::to_string(s.layout.y) + '#' + std::to_string(uistyle::generation());
    for (const gc::Member& m : s.members)
        key += ';' + m.name + ',' + m.title + ',' + std::to_string(m.level) + (m.online ? "o" : "");
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
    const panel::Box& gl = s.layout.glass;
    s.shadow.draw(record, renderer, s.layout.x + int(gl.x), s.layout.y + int(gl.y), int(gl.w), int(gl.h),
                  12 * s.scale, s.scale, s.k);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m == WM_MOUSEWHEEL) {
        const int by = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1;
        s.show.scroll = std::clamp(s.show.scroll + by, 0, gc::mostScroll(s.layout, s.members.size()));
        s.key.clear();
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    const gc::Hit h = gc::hitTest(s.layout, lx, ly);
    if (h.part == gc::Part::Tab) {
        s.show.tab = gc::Tab(h.index);
        s.show.scroll = 0;
        s.key.clear();
    } else if (h.part == gc::Part::Close) {
        closeIt(s);
    }
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    closeIt(s);
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameGuild(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    switch (c) {
        case PadCommand::Up: s.show.scroll = std::max(0, s.show.scroll - 1); break;
        case PadCommand::Down:
            s.show.scroll = std::min(gc::mostScroll(s.layout, s.members.size()), s.show.scroll + 1);
            break;
        case PadCommand::TabBack:
        case PadCommand::TabOn:
            s.show.tab = s.show.tab == gc::Tab::Info ? gc::Tab::Members : gc::Tab::Info;
            s.show.scroll = 0;
            break;
        case PadCommand::Close: closeIt(s); break;
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

}  // namespace uagfx::win::guildcardwin
