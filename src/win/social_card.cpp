#include "win/social_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/settings/strings.hpp"
#include "core/social_card.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::socialcardwin {
namespace {

namespace sc = uagfx::socialcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the friends and party window (where the manager keeps it, its class's vtable, its SendMsg), a window's
// shown flag, its tab, its list (a node: next +0, valid +8, the name's words +0x18 / +0x1C, the map's +0x28 / +0x2C,
// the role +0x34 - 0 the leader -, offline +0x38) and the chosen row; the party's name the game keeps (a char*)
constexpr uint32_t kWindowMgr = 0x727ED8, kSocialAt = kWindowMgr + 0x11C, kSocialClass = 0x69BBB4;
constexpr uint32_t kSocialSend = 0x4C2250, kShown = 0x38, kSendMsgSlot = 0x94, kTab = 0xC0, kList = 0xA4;
constexpr uint32_t kChosen = 0xAC, kPartyName = 0x772D88;
constexpr int kButtonMsg = 6, kMostFriends = 40;
constexpr size_t kMostNodes = 100;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hidden = 0;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::vector<sc::Entry> entries;
    std::string partyName;
    sc::Show show;
    sc::Layout layout;
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

uint32_t gameSocial() {
    const uint32_t w = read32(kSocialAt);
    return w && read32(w) == kSocialClass && read32(read32(w) + kSendMsgSlot) == kSocialSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("social: the game's friends and party window hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

std::string words(uint32_t ptr, uint32_t len) {
    if (!ptr || !len || len > 64) return "";
    char buf[65] = {};
    return readGame(ptr, buf, len) ? std::string(buf, len) : "";
}

std::vector<sc::Entry> readList(const State& s, uint32_t window, bool party) {
    std::vector<sc::Entry> out;
    const std::string own = readOwnName();
    const uint32_t head = read32(window + kList);
    if (!head) return out;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        uint8_t raw[0x40] = {};
        if (!readGame(node, raw, sizeof raw)) break;
        uint32_t f[16];
        std::memcpy(f, raw, sizeof f);
        sc::Entry e;
        const std::string name = words(f[6], f[7]);
        std::string map = words(f[10], f[11]);
        if (map.size() > 4 && map.compare(map.size() - 4, 4, ".gat") == 0) map.resize(map.size() - 4);
        e.name = utf8Of(widened(name, s.codePage));
        e.map = utf8Of(widened(map, s.codePage));
        e.leader = party && f[13] == 0;
        e.online = f[14] == 0;
        e.self = !own.empty() && name == own;
        out.push_back(std::move(e));
    }
    return out;
}

sc::Texts texts(const State& s) {
    sc::Texts t;
    t.tabs[0] = say(settings::Text::SocialFriends);
    t.tabs[1] = say(settings::Text::SocialParty);
    t.offline = say(settings::Text::SocialOffline);
    t.empty = say(settings::Text::SocialEmpty);
    if (s.show.tab == sc::Tab::Party) {
        t.header = s.partyName.empty() ? say(settings::Text::SocialNoParty)
                                       : settings::fmt(say(settings::Text::SocialPartyName), {s.partyName});
        t.actions[0] = say(settings::Text::SocialWhisper);
        t.actions[1] = say(settings::Text::SocialExpel);
        t.actions[2] = say(settings::Text::SocialLeave);
        t.actions[3] = say(settings::Text::SocialSettings);
    } else {
        t.header = settings::fmt(say(settings::Text::SocialFriendCount),
                                 {std::to_string(s.entries.size()), std::to_string(kMostFriends)});
        t.actions[0] = say(settings::Text::SocialWhisper);
        t.actions[1] = say(settings::Text::SocialDelete);
        t.actions[2] = say(settings::Text::SocialSettings);
    }
    return t;
}

std::vector<Words> gdiWords(const std::vector<sc::Words>& in) {
    std::vector<Words> out;
    for (const sc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == sc::Align::Right    ? DT_RIGHT
                                 : w.align == sc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the friends and the party", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    sc::paint(s.picture, s.layout, s.entries, s.show);
    drawWords(s.picture, gdiWords(sc::words(s.layout, s.entries, s.show, texts(s))));
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
    uapad::gamewindow::setBlocked(Blocker::Social, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Social, nullptr, nullptr, false);
    s.shown = false;
}

void send(State& s, int id, int b, const char* what) {
    const uint32_t window = gameSocial();
    if (!window) return;
    if (s.told++ < 40) LOG_INFO("social: %s - the window's 0x%X", what, unsigned(id));
    reinterpret_cast<SendMsgFn>(uintptr_t(kSocialSend))(window, 0, kButtonMsg, id, b, 0);
    s.key.clear();
}

void choose(State& s, int row) {
    const uint32_t window = gameSocial();
    if (!window || row < 0 || row >= int(s.entries.size())) return;
    write32(window + kChosen, uint32_t(row));
    s.show.selected = row;
    s.key.clear();
}

void act(State& s, int button) {
    const std::vector<sc::Action> acts = sc::actions(s.show.tab);
    if (button < 0 || button >= int(acts.size())) return;
    const sc::Action a = acts[size_t(button)];
    const sc::Entry* chosen =
        s.show.selected >= 0 && s.show.selected < int(s.entries.size()) ? &s.entries[size_t(s.show.selected)] : nullptr;
    if (!sc::allowed(s.show.tab, a, chosen)) return;
    if (a == sc::Action::Leave) {  // the game leaves only with one's own row chosen
        for (size_t i = 0; i < s.entries.size(); ++i)
            if (s.entries[i].self) choose(s, int(i));
    }
    const char* const names[5] = {"whisper", "delete or expel (the game asks)", "leave (the game asks)", "settings",
                                  "close"};
    send(s, sc::gameButton(a), 0, names[int(a)]);
}

void showTab(State& s, sc::Tab tab) {
    if (tab == s.show.tab) return;
    send(s, sc::kTabMsg, sc::tabParam(tab), tab == sc::Tab::Party ? "the party's tab" : "the friends' tab");
    s.show.selected = -1;
    s.show.scroll = 0;
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("social: [interface] social = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameSocial() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) {
        s.show = sc::Show{};
        LOG_INFO("social: the game's friends and party window - the new card shows it");
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.show.tab = read32(window + kTab) ? sc::Tab::Party : sc::Tab::Friends;
    s.entries = readList(s, window, s.show.tab == sc::Tab::Party);
    const uint32_t partyName = read32(kPartyName);
    char name[25] = {};
    s.partyName = partyName && readGame(partyName, name, 24) ? utf8Of(widened(name, s.codePage)) : "";
    s.show.selected = int32_t(read32(window + kChosen));
    if (s.show.selected >= int(s.entries.size())) s.show.selected = -1;
    s.layout = sc::layout(targetWidth, targetHeight, s.scale, s.show.tab, s.entries.size());
    s.show.scroll = std::clamp(s.show.scroll, 0, sc::mostScroll(s.layout, s.entries.size()));
    s.show.hoverRow = s.show.hoverButton = s.show.hoverTab = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const float lx = float(cx) * float(k) - float(s.layout.x), ly = float(cy) * float(k) - float(s.layout.y);
        const sc::Hit h = sc::hitTest(s.layout, s.show.scroll, s.entries.size(), lx, ly);
        if (h.part == sc::Part::Row) s.show.hoverRow = h.index;
        if (h.part == sc::Part::Button) s.show.hoverButton = h.index;
        if (h.part == sc::Part::Tab) s.show.hoverTab = h.index;
        s.show.hoverClose = h.part == sc::Part::Close;
    }
    std::string key = std::to_string(int(s.show.tab)) + ';' + std::to_string(s.show.selected) + ',' +
                      std::to_string(s.show.scroll) + ',' + std::to_string(s.show.hoverRow) + ',' +
                      std::to_string(s.show.hoverButton) + ',' + std::to_string(s.show.hoverTab) +
                      (s.show.hoverClose ? "x" : "") + ',' + std::to_string(s.show.focus) + ';' + s.partyName + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                      std::to_string(uistyle::generation());
    for (const sc::Entry& e : s.entries)
        key += ';' + e.name + ',' + e.map + (e.online ? "o" : "") + (e.leader ? "l" : "") + (e.self ? "s" : "");
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
        s.show.scroll = std::clamp(s.show.scroll + by, 0, sc::mostScroll(s.layout, s.entries.size()));
        s.key.clear();
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    const sc::Hit h = sc::hitTest(s.layout, s.show.scroll, s.entries.size(), lx, ly);
    switch (h.part) {
        case sc::Part::Tab: showTab(s, sc::Tab(h.index)); break;
        case sc::Part::Row:
            choose(s, h.index);
            if (m == WM_LBUTTONDBLCLK) act(s, 0);  // twice on a person: the whisper to them
            break;
        case sc::Part::Button: act(s, h.index); break;
        case sc::Part::Close: send(s, sc::gameButton(sc::Action::Close), 0, "close"); break;
        case sc::Part::None: break;
    }
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    send(s, sc::gameButton(sc::Action::Close), 0, "Esc - close");
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameSocial(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    const int rows = int(s.entries.size()), buttons = int(sc::actions(s.show.tab).size());
    int& f = s.show.focus;
    if (f < 0) f = 0;
    switch (c) {
        case PadCommand::Up: choose(s, std::max(0, s.show.selected - 1)); break;
        case PadCommand::Down: choose(s, std::min(rows - 1, s.show.selected + 1)); break;
        case PadCommand::Left: f = std::max(0, f - 1); break;
        case PadCommand::Right: f = std::min(buttons - 1, f + 1); break;
        case PadCommand::Press: act(s, f); break;
        case PadCommand::TabBack:
        case PadCommand::TabOn:
            showTab(s, s.show.tab == sc::Tab::Party ? sc::Tab::Friends : sc::Tab::Party);
            f = 0;
            break;
        case PadCommand::Close: send(s, sc::gameButton(sc::Action::Close), 0, "the controller's close"); break;
    }
    if (s.show.selected >= 0) {
        if (s.show.selected < s.show.scroll) s.show.scroll = s.show.selected;
        if (s.show.selected >= s.show.scroll + int(s.layout.rows.size()))
            s.show.scroll = s.show.selected - int(s.layout.rows.size()) + 1;
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

}  // namespace uagfx::win::socialcardwin
