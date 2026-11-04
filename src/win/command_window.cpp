#include "win/command_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/command_card.hpp"
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
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
using KeyFn = int(__attribute__((thiscall)) *)(uint32_t manager, int key, int how);

struct State {
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

std::string say(settings::Text t) { return settings::tr(language(), t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
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

std::vector<Words> gdiWords(const std::vector<cc::Words>& in) {
    std::vector<Words> out;
    for (const cc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == cc::Align::Right    ? DT_RIGHT
                                 : w.align == cc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

const std::vector<cc::Entry>& shownList(const State& s) { return s.lists[int(s.show.tab)]; }

void render(State& s) {
    const uicost::Paint cost("the command list", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    cc::paint(s.picture, s.layout, shownList(s), s.show);
    drawWords(s.picture, gdiWords(cc::words(s.layout, shownList(s), s.show, texts())));
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

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
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
    if (readGameCursor(cx, cy)) {
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
    const cc::Hit h = cc::hitTest(s.layout, lx, ly);
    switch (h.part) {
        case cc::Part::Close: close(); break;
        case cc::Part::Tab:
            s.show.tab = cc::Tab(std::clamp(h.index, 0, 2));
            s.key.clear();
            break;
        case cc::Part::Row:
            if (m == WM_LBUTTONDOWN) use(s, h.index);  // a double click's second press not a second use
            break;
        case cc::Part::None: break;
    }
    return true;
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
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::commandwin
