#include "win/command_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/window_stack.hpp"
#include "core/command_card.hpp"
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

namespace uagfx::win::commandwin {
namespace {

namespace cc = uagfx::commandcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the command window (where the manager keeps it, its class's vtable, its SendMsg; its shown flag, its
// tabs' control and that one's chosen tab); the session's lists (the first, past the last; 8 bytes each); the game
// mode (its message 0x2F a command by its code) and the windows' manager's keys (thiscall: the key, 10000 as with Alt)
constexpr uint32_t kWindowMgr = 0x727ED8, kCmdAt = kWindowMgr + 0x1E0, kCmdClass = 0x69A2F0, kCmdSend = 0x48EEE0;
constexpr uint32_t kSendMsgSlot = 0x94, kShown = 0x38, kTabsAt = 0x60, kTabChosen = 0x30;
constexpr uint32_t kSession = 0x771148, kSwitches = kSession + 0x1914, kCommands = kSession + 0x1924;
constexpr uint32_t kStates = kSession + 0x1934, kKeys = kSession + 0x1944;
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0, kKeyHandler = 0x4D96E0;
constexpr int kButtonMsg = 6, kCloseButton = 0x87, kCommandMsg = 0x2F, kHi = 0x96, kAsAlt = 10000, kMost = 64;
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
using KeyFn = int(__attribute__((thiscall)) *)(uint32_t manager, int key, int how);

// 3.0.18-beta.73 (the big six, F): the window a live document - tabs and rows by flexbox, RmlUi's clicks
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw cw" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span></div>
<span class="x ctl" data-event-click="shut()">&#x2715;</span>
<div class="tabs"><span class="tab ctl" data-for="t, i : tabs" data-class-on="i == tab"
  data-event-click="tab_pick(i)">{{t}}</span></div>
<div class="grid"><div class="row ctl" data-for="e, i : rows" data-event-click="use_row(i)"><span class="key"
  data-if="e.key != ''">{{e.key}}</span><span class="name">{{e.text}}</span><span class="state" data-if="e.state >= 0"
  data-class-on="e.state > 0">{{e.state > 0 ? on : off}}</span></div></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";

struct RowView {
    Rml::String key, text;
    int state = -1;
};

struct State {
    uilive::Window live;  // 3.0.18-beta.73
    std::vector<RowView> rows;
    std::vector<Rml::String> tabs;
    int tab = 0;
    Rml::String title, onWord, offWord, ground = "#10141cf0";
    uisheet::Sheet sheet;  // 3.0.18-beta.32: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false, hid = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::vector<cc::Entry> lists[3];
    cc::Show show;
    cc::Layout layout;
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

uint32_t gameCommands() {
    const uint32_t w = read32(kCmdAt);
    return w && read32(w) == kCmdClass && read32(kCmdClass + kSendMsgSlot) == kCmdSend ? w : 0;
}

// The session's list at `first` (its past-the-last next): pairs of a word's address and a code
std::vector<std::pair<uint32_t, int>> pairs(uint32_t first) {
    std::vector<std::pair<uint32_t, int>> out;
    const uint32_t from = read32(first), past = read32(first + 4);
    if (!from || past <= from || (past - from) / 8 > uint32_t(kMost)) return out;
    for (uint32_t at = from; at < past; at += 8) out.emplace_back(read32(at), int(read32(at + 4)));
    return out;
}

std::string words(const State& s, uint32_t at) {
    char buf[64] = {};
    if (!at || !readGame(at, buf, sizeof buf - 1)) return "";
    buf[sizeof buf - 1] = 0;
    return utf8Of(widened(buf, s.codePage));
}

// A window's name by its key - the driver's words; another key the game's own
std::string windowName(int vk, const std::string& game) {
    using settings::Text;
    switch (vk) {
        case 'C': return say(Text::CmdWinChat);
        case 'A': return say(Text::CmdWinStatus);
        case 'D': return say(Text::CmdWinDetail);
        case 'O': return say(Text::CmdWinOptions);
        case 'E': return say(Text::CmdWinItems);
        case 'W': return say(Text::CmdWinCart);
        case 'S': return say(Text::CmdWinSkills);
        case 'Q': return say(Text::CmdWinEquip);
        case 'V': return say(Text::CmdWinBasic);
        case 'G': return say(Text::CmdWinGuild);
        case 'P': return say(Text::CmdWinParty);
        case 'I': return say(Text::CmdWinFriendSetup);
        case 'Z': return say(Text::CmdWinFriends);
        case 'H': return say(Text::CmdWinWhispers);
        case 'M': return say(Text::CmdWinMacros);
        case 'L': return say(Text::CmdWinEmotions);
        case 'J': return say(Text::CmdWinPet);
        default: return game;
    }
}

void readLists(State& s) {
    std::vector<std::pair<int, int>> states;
    for (const auto& [state, code] : pairs(kStates)) states.emplace_back(int(state), code);
    for (int list = 0; list < 2; ++list) {
        std::vector<cc::Entry>& out = s.lists[list];
        out.clear();
        for (const auto& [at, code] : pairs(list == 0 ? kSwitches : kCommands))
            out.push_back(cc::Entry{words(s, at), code, -1});
        cc::markStates(out, states);
    }
    s.lists[2].clear();
    for (const auto& [at, vk] : pairs(kKeys)) s.lists[2].push_back(cc::Entry{windowName(vk, words(s, at)), vk, -1});
}

cc::Texts texts() {
    cc::Texts t;
    t.title = say(settings::Text::CmdTitle);
    t.tabs[0] = say(settings::Text::CmdTabSwitches);
    t.tabs[1] = say(settings::Text::CmdTabCommands);
    t.tabs[2] = say(settings::Text::CmdTabWindows);
    t.on = say(settings::Text::CmdOn);
    t.off = say(settings::Text::CmdOff);
    return t;
}

const std::vector<cc::Entry>& shownList(const State& s) { return s.lists[int(s.show.tab)]; }

void render(State& s) {
    ++s.sheetGen;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    panel::Box b = s.layout.glass;
    float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    int lx = 0, ly = 0, lw = 0, lh = 0;  // 3.0.18-beta.73: the live window's own box
    if (s.live.box(lx, ly, lw, lh)) x0 = float(lx), y0 = float(ly), b.w = float(lw), b.h = float(lh);
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
    uapad::gamewindow::setBlocked(Blocker::Commands, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Commands, nullptr, nullptr, false);
    s.shown = false;
}

// The entry `i` of the shown tab, as the game's click does it
void use(State& s, int i) {
    const std::vector<cc::Entry>& list = shownList(s);
    if (i < 0 || i >= int(list.size()) || !gameCommands() || !inGameWorld()) return;
    const cc::Entry& e = list[size_t(i)];
    if (s.show.tab == cc::Tab::Windows) {
        if (s.told++ < 30) LOG_INFO("commands: %s - the window's key Alt+%c", e.text.c_str(), char(e.code));
        reinterpret_cast<KeyFn>(uintptr_t(kKeyHandler))(kWindowMgr, e.code, kAsAlt);
        s.key.clear();
        return;
    }
    const uint32_t mode = read32(kModeNow);
    if (!mode || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return;
    if (s.told++ < 30) LOG_INFO("commands: %s - the game's command 0x%X", e.text.c_str(), unsigned(e.code));
    if (e.code == kHi) {  // /hi: three words after it (the greeting's), empty - as the game's click
        uint32_t none[12] = {};
        const int greeting = int(reinterpret_cast<uintptr_t>(none));
        reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, kCommandMsg, e.code, greeting, 0);
    } else {
        reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, kCommandMsg, e.code, 0, 0);
    }
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
        LOG_INFO("commands: [interface] commands = %d", on ? 1 : 0);
    }
    const uint32_t window = gameCommands();
    if (window) {  // the game's window hidden while ours shows; back when ours is off
        if (on && read32(window + kShown) && write32(window + kShown, 0)) s.hid = true;
        else if (!on && s.hid && write32(window + kShown, 1)) s.hid = false;
    } else {
        s.hid = false;
    }
    if (!on || !window || !inGameWorld() || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) {  // its tab as the game's window has it
        const uint32_t tab = read32(read32(window + kTabsAt) + kTabChosen);
        s.show.tab = tab <= 2 ? cc::Tab(tab) : cc::Tab::Switches;
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    readLists(s);
    if (!s.shown)
        LOG_INFO("commands: the game's command list (Alt+Y) - the new card shows it (%zu switches, %zu commands, %zu "
                 "windows)",
                 s.lists[0].size(), s.lists[1].size(), s.lists[2].size());
    const size_t most = std::max({s.lists[0].size(), s.lists[1].size(), s.lists[2].size()});
    s.layout = cc::layout(most, targetWidth, targetHeight, s.scale);
    s.show.hover = s.show.hoverTab = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) {
        const cc::Hit h = cc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == cc::Part::Row && h.index < int(shownList(s).size())) s.show.hover = h.index;
        if (h.part == cc::Part::Tab) s.show.hoverTab = h.index;
        s.show.hoverClose = h.part == cc::Part::Close;
    }
    std::string key = std::to_string(int(s.show.tab)) + ';' + std::to_string(s.show.hover) + ',' +
                      std::to_string(s.show.hoverTab) + (s.show.hoverClose ? "x" : "") + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ',' + std::to_string(s.layout.w) +
                      '#' + std::to_string(uistyle::generation());
    for (const cc::Entry& e : shownList(s)) key += ';' + e.text + std::to_string(e.state);
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.32
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.73: the live window's model and place
static void liveFrame(State& s) {
    s.live.make("commands", kLive, [&](Rml::DataModelConstructor& m) {
        if (auto v = m.RegisterStruct<RowView>()) {
            v.RegisterMember("key", &RowView::key);
            v.RegisterMember("text", &RowView::text);
            v.RegisterMember("state", &RowView::state);
        }
        m.RegisterArray<std::vector<RowView>>();
        m.RegisterArray<std::vector<Rml::String>>();
        m.Bind("rows", &s.rows);
        m.Bind("tabs", &s.tabs);
        m.Bind("tab", &s.tab);
        m.Bind("title", &s.title);
        m.Bind("on", &s.onWord);
        m.Bind("off", &s.offWord);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("tab_pick", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (a.empty()) return;
            State& st = state();
            st.show.tab = cc::Tab(std::clamp(a[0].Get<int>(), 0, 2));
            st.key.clear();
        });
        m.BindEventCallback("use_row", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (!a.empty()) use(state(), a[0].Get<int>());
        });
        m.BindEventCallback("shut", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { close(); });
    });
    if (!s.live.made()) return;
    const cc::Texts t = texts();
    std::vector<RowView> rows;
    const bool keys = s.show.tab == cc::Tab::Windows;
    for (const cc::Entry& e : shownList(s)) rows.push_back(RowView{keys ? cc::keyName(e.code) : "", e.text, e.state});
    const bool same = rows.size() == s.rows.size() &&
                      std::equal(rows.begin(), rows.end(), s.rows.begin(), [](const RowView& a, const RowView& b) {
                          return a.key == b.key && a.text == b.text && a.state == b.state;
                      });
    if (!same) s.rows = std::move(rows), s.live.doc.dirty("rows");
    const std::vector<Rml::String> tabs{t.tabs[0], t.tabs[1], t.tabs[2]};
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    if (tabs != s.tabs || int(s.show.tab) != s.tab || t.title != s.title || t.on != s.onWord || t.off != s.offWord ||
        ground != s.ground) {
        s.tabs = tabs, s.tab = int(s.show.tab), s.title = t.title, s.onWord = t.on, s.offWord = t.off, s.ground = ground;
        s.live.doc.dirtyAll();
    }
    s.live.frame(s.shown, s.layout.x, s.layout.y, int(winstack::Win::Commands), ".row, .tab, .x");
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    liveFrame(s);  // the old edition (the sheet) gone: the live window alone
    if (s.sheetShown) {  // 3.0.18-beta.60: the pad walks its boxes (A clicks one)
        std::vector<panel::Box> targets;
        for (const auto& part : std::initializer_list<std::vector<panel::Box>>{s.layout.tabs, s.layout.rows, {s.layout.close}})
            targets.insert(targets.end(), part.begin(), part.end());
        s.sheet.nav(int(winstack::Win::Commands), std::move(targets));
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.live.shown) return;  // 3.0.18-beta.73
    s.live.draw(s.shadow, record, renderer, s.scale, s.k);
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL) return false;
    if (s.live.shown) {  // 3.0.18-beta.73: RmlUi's
        if (!s.live.mouse(game, m, wp, lp)) return false;
        result = 0;
        return true;
    }
    return false;  // the old edition gone: the live window alone
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    const uint32_t w = gameCommands();
    if (!s.shown || !w) return false;
    if (s.told++ < 30) LOG_INFO("commands: closed (the window's 0x87)");
    reinterpret_cast<SendMsgFn>(uintptr_t(kCmdSend))(w, 0, kButtonMsg, kCloseButton, 0, 0);
    s.key.clear();
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (!s.on) return;
    const uint32_t w = gameCommands();
    if (w && read32(w + kShown) && write32(w + kShown, 0)) s.hid = true;
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    if (s.live.box(x, y, w, h)) return true;  // 3.0.18-beta.73
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::commandwin
