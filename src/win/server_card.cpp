#include "win/server_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_stack.hpp"
#include "core/pick_card.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"
#include "win/words.hpp"

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

// 3.0.18-beta.106 (the owner: «делай 1-3», the login screens): the server select live - its rows clicked (a double
// click goes on), «Далі» and «Назад» RmlUi's; the game's list behind it as before
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw pkw svw" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span></div>
<div class="msg" data-if="message != ''">{{message}}</div>
<div class="list"><div class="row ctl" data-for="e, i : entries" data-class-on="i == selected"
  data-event-click="sv_choose(i)" data-event-dblclick="sv_ok()"><span class="name">{{e}}</span></div>
<div class="hint" data-if="entries.size == 0">{{empty}}</div></div>
<div class="btns"><div class="btn ctl" data-event-click="sv_cancel()">{{cancel}}</div>
<div class="btn main ctl" data-class-off="selected == -1" data-event-click="sv_ok()">{{ok}}</div></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";

struct State {
    uilive::Window live;  // 3.0.18-beta.81
    std::vector<Rml::String> entryViews;  // 3.0.18-beta.106: the live window's
    std::vector<packets::ServerList::Server> listed;  // 3.0.18-beta.153: the login server's list (its players)
    Rml::String title, message, ok, cancel, empty, ground = "#00000000";
    int selected = -1;
    uisheet::Sheet sheet;  // 3.0.18-beta.33: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
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

void render(State& s) {
    ++s.sheetGen;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    panel::Box b = s.layout.glass;
    float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    int lx = 0, ly = 0, lw = 0, lh = 0;  // 3.0.18-beta.81: the live window's own box
    if (s.live.box(lx, ly, lw, lh)) x0 = float(lx), y0 = float(ly), b.w = float(lw), b.h = float(lh);
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
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

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
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
    if (readHoverCursor(cx, cy)) {
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
    if (const std::string drawn = key + (uaui::readyAnywhere() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.33
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.81: the live window's model and place
static void liveFrame(State& s) {
    s.live.anywhere = true;  // before the world
    s.live.make("server", kLive, [&](Rml::DataModelConstructor& m) {
        m.RegisterArray<std::vector<Rml::String>>();
        m.Bind("entries", &s.entryViews);
        m.Bind("title", &s.title);
        m.Bind("message", &s.message);
        m.Bind("ok", &s.ok);
        m.Bind("cancel", &s.cancel);
        m.Bind("empty", &s.empty);
        m.Bind("selected", &s.selected);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("sv_choose", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (!a.empty()) choose(state(), a[0].Get<int>());
        });
        m.BindEventCallback("sv_ok", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            press(state(), pc::kOkButton);
        });
        m.BindEventCallback("sv_cancel", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            press(state(), pc::kCancelButton);
        });
    });
    if (!s.live.made()) return;
    const pc::Texts t = texts();
    std::vector<Rml::String> entries;
    for (const pc::Entry& e : s.entries) {
        std::string row = e.name;
        for (const auto& t2 : s.listed) {  // 3.0.18-beta.153: its players, as the login server told them
            const std::string name = utf8Of(widened(t2.name, s.codePage));
            if (name.empty() || row.rfind(name, 0) != 0) continue;
            if (row.find(std::to_string(t2.players)) == std::string::npos)
                row += "  \xC2\xB7  " +
                       settings::fmt(say(settings::Text::ServersPlayers), {std::to_string(t2.players)});
            break;
        }
        entries.push_back(row);
    }
    if (entries != s.entryViews) s.entryViews = std::move(entries), s.live.doc.dirty("entries");
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    if (t.title != s.title || t.message != s.message || t.ok != s.ok || t.cancel != s.cancel || t.empty != s.empty ||
        s.show.selected != s.selected || ground != s.ground) {
        s.title = t.title, s.message = t.message, s.ok = t.ok, s.cancel = t.cancel, s.empty = t.empty;
        s.selected = s.show.selected, s.ground = ground;
        s.live.doc.dirtyAll();
    }
    int x = s.layout.x, y = s.layout.y, bx = 0, by = 0, bw = 0, bh = 0;
    if (s.live.box(bx, by, bw, bh)) x = (s.targetW - bw) / 2, y = std::max(0, s.targetH / 3 - bh / 2);
    s.live.frame(s.shown, x, y, int(winstack::Win::Servers), ".row, .btn");
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    liveFrame(s);  // the old edition (the sheet) gone: the live window alone
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.live.shown) return;  // 3.0.18-beta.81
    s.live.draw(s.shadow, record, renderer, s.scale, s.k);
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (s.live.shown) {  // 3.0.18-beta.81: RmlUi's
        if (!s.live.mouse(game, m, wp, lp)) return false;
        result = 0;
        return true;
    }
    return false;  // the old edition gone: the live window alone
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
    if (s.live.box(x, y, w, h)) return true;  // 3.0.18-beta.81
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

void packet(const packets::Event& e) {  // 3.0.18-beta.153
    if (const auto* l = std::get_if<packets::ServerList>(&e)) {
        state().listed = l->servers;
        LOG_INFO("servers: the login server's list - %zu server(s)", l->servers.size());
    }
}

}  // namespace uagfx::win::servercardwin
