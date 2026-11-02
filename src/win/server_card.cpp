#include "win/server_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/pick_card.hpp"
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

namespace uagfx::win::servercardwin {
namespace {

namespace pc = uagfx::pickcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the manager's windows' list (a node: next +0, the window +8), the server select's class and SendMsg,
// its list control and that control's items (16 bytes each: the words' pointer +4, their count +8), their end, the
// chosen one; a window's place (+0x14)
constexpr uint32_t kWindowMgr = 0x727ED8, kList = 0x38, kServerClass = 0x6979D8, kServerSend = 0x4563C0;
constexpr uint32_t kControl = 0x60, kItems = 0x58, kItemsEnd = 0x5C, kChosen = 0x64, kRect = 0x14, kSendMsgSlot = 0x94;
constexpr int kButtonMsg = 6, kAway = -4000;
constexpr size_t kMostWindows = 128, kMostServers = 32;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
    WindowShadow shadow;
    bool on = false, shown = false, moved = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0, oldX = 0;
    uint32_t window = 0;
    std::vector<pc::Entry> entries;
    pc::Show show;
    pc::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
    bool pressed = false;  // 3.0.11-beta.171: the only server confirmed for this window
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

// The server select in the manager's list (kept nowhere else); none in the world
uint32_t gameServers() {
    if (inGameWorld()) return 0;
    const uint32_t head = read32(kWindowMgr + kList);
    if (!head) return 0;
    uint32_t node = read32(head);
    for (size_t n = 0; n < kMostWindows && node && node != head; ++n) {
        const uint32_t w = read32(node + 8);
        if (w && read32(w) == kServerClass && read32(read32(w) + kSendMsgSlot) == kServerSend) return w;
        node = read32(node);
    }
    return 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.moved = false;
        return;
    }
    const int32_t x = int32_t(read32(window + kRect));
    if (on && x != kAway) {
        s.oldX = x;
        write32(window + kRect, uint32_t(kAway));
        if (!s.moved) LOG_INFO("servers: the game's server select off the screen for the new card");
        s.moved = true;
    } else if (!on && s.moved) {
        write32(window + kRect, uint32_t(s.oldX));
        s.moved = false;
    }
}

std::vector<pc::Entry> readServers(const State& s, uint32_t control) {
    std::vector<pc::Entry> out;
    const uint32_t base = read32(control + kItems), end = read32(control + kItemsEnd);
    if (!base || end < base) return out;
    const size_t count = std::min<size_t>((end - base) / 16, kMostServers);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t ptr = read32(base + uint32_t(i) * 16 + 4), len = read32(base + uint32_t(i) * 16 + 8);
        char buf[129] = {};
        pc::Entry e;
        if (ptr && len && len < sizeof buf && readGame(ptr, buf, len)) e.name = utf8Of(widened(buf, s.codePage));
        out.push_back(std::move(e));
    }
    return out;
}

pc::Texts texts() {
    pc::Texts t;
    t.title = say(settings::Text::ServersTitle);
    t.message = say(settings::Text::ServersHint);
    t.ok = say(settings::Text::ServersNext);
    t.cancel = say(settings::Text::CharsBack);
    t.empty = say(settings::Text::ServersEmpty);
    return t;
}

std::vector<Words> gdiWords(const std::vector<pc::Words>& in) {
    std::vector<Words> out;
    for (const pc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == pc::Align::Right    ? DT_RIGHT
                                 : w.align == pc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the server select", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    pc::paint(s.picture, s.layout, s.entries, s.show);
    drawWords(s.picture, gdiWords(pc::words(s.layout, s.entries, s.show, texts())));
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
    uapad::gamewindow::setBlocked(Blocker::Servers, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Servers, nullptr, nullptr, false);
    s.shown = false;
}

void choose(State& s, int row) {
    const uint32_t window = gameServers(), control = window ? read32(window + kControl) : 0;
    if (!control || row < 0 || row >= int(s.entries.size())) return;
    write32(control + kChosen, uint32_t(row));
    s.show.selected = row;
    s.key.clear();
}

void press(State& s, int id) {
    const uint32_t window = gameServers();
    if (!window) return;
    if (id == pc::kOkButton && s.show.selected < 0) return;
    if (s.told++ < 30)
        LOG_INFO("servers: %s - the game's 0x%X", id == pc::kOkButton ? "«Далі»" : "«Назад»", unsigned(id));
    reinterpret_cast<SendMsgFn>(uintptr_t(kServerSend))(window, 0, kButtonMsg, id, 0, 0);
    s.key.clear();
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("servers: [interface] servers = %d", on ? 1 : 0);
    }
    const uint32_t window = gameServers();
    hold(s, window, on);
    const uint32_t control = window ? read32(window + kControl) : 0;
    if (!on || !window || !control || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        s.window = 0;
        return;
    }
    if (window != s.window) s.pressed = false;
    const bool fresh = !s.shown || window != s.window;
    if (fresh) s.show = pc::Show{};
    s.window = window;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.entries = readServers(s, control);
    s.show.selected = int32_t(read32(control + kChosen));
    if (s.show.selected >= int(s.entries.size())) s.show.selected = -1;
    if (pc::autoChoice(s.entries.size(), s.show.selected) == 0) {  // 3.0.11-beta.171: the only one - on at once
        if (!s.pressed) {
            s.pressed = true;
            choose(s, 0);
            LOG_INFO("servers: the only server - chosen at once");
            press(s, pc::kOkButton);
        }
        hide(s);
        return;
    }
    if (fresh) LOG_INFO("servers: the game's server select - the new card shows it");
    s.layout = pc::layout(targetWidth, targetHeight, s.scale, s.entries.size());
    s.show.scroll = std::clamp(s.show.scroll, 0, pc::mostScroll(s.layout, s.entries.size()));
    s.show.hover = -1;
    s.show.hoverPart = 0;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const float lx = float(cx) * float(k) - float(s.layout.x), ly = float(cy) * float(k) - float(s.layout.y);
        const pc::Hit h = pc::hitTest(s.layout, s.show.scroll, s.entries.size(), lx, ly);
        s.show.hover = h.row;
        s.show.hoverPart = int(h.part);
    }
    std::string key = std::to_string(s.show.selected) + ',' + std::to_string(s.show.hover) + ',' +
                      std::to_string(s.show.hoverPart) + ',' + std::to_string(s.show.scroll) + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                      std::to_string(uistyle::generation());
    for (const pc::Entry& e : s.entries) key += ';' + e.name;
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
        s.show.scroll = std::clamp(s.show.scroll + by, 0, pc::mostScroll(s.layout, s.entries.size()));
        s.key.clear();
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    const pc::Hit h = pc::hitTest(s.layout, s.show.scroll, s.entries.size(), lx, ly);
    switch (h.part) {
        case pc::Part::Row:
            choose(s, h.row);
            if (m == WM_LBUTTONDBLCLK) press(s, pc::kOkButton);
            break;
        case pc::Part::Ok: press(s, pc::kOkButton); break;
        case pc::Part::Cancel: press(s, pc::kCancelButton); break;
        case pc::Part::None: break;
    }
    return true;
}

bool shown() { return state().shown; }

void beforeGameDraw() {
    State& s = state();
    if (s.on) hold(s, gameServers(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    const int n = int(s.entries.size());
    switch (c) {
        case PadCommand::Up: choose(s, std::max(0, s.show.selected - 1)); break;
        case PadCommand::Down: choose(s, std::min(n - 1, s.show.selected + 1)); break;
        case PadCommand::Ok: press(s, pc::kOkButton); break;
        case PadCommand::Back: press(s, pc::kCancelButton); break;
    }
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

}  // namespace uagfx::win::servercardwin
