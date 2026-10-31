#include "pad/win/runtime.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "core/input_mode.hpp"
#include "core/layout.hpp"
#include "core/layout_migration.hpp"
#include "pad/core/bytes.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/core/config/config.hpp"
#include "pad/core/config/ini_edit.hpp"
#include "pad/core/config/settings.hpp"
#include "pad/core/i18n.hpp"
#include "pad/core/logic/cursor_bridge.hpp"
#include "pad/core/logic/mouse_gate.hpp"
#include "pad/core/logic/npc_hint.hpp"
#include "pad/core/logic/settings_menu.hpp"
#include "pad/core/logic/vbutton.hpp"
#include "pad/core/logic/window_order.hpp"
#include "pad/core/session.hpp"
#include "pad/core/version.hpp"
#include "pad/win/client_bar.hpp"
#include "pad/win/client_camera.hpp"
#include "pad/win/client_mouse.hpp"
#include "pad/win/client_npc.hpp"
#include "pad/win/hint.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/hooks.hpp"
#include "pad/win/log.hpp"
#include "pad/win/overlay.hpp"
#include "pad/win/pads.hpp"
#include "win/disk.hpp"
#include "win/paths.hpp"
#include "core/walk_path.hpp"
#include "win/char_block.hpp"
#include "win/game_camera.hpp"
#include "win/item_window.hpp"
#include "win/minimap_proto.hpp"
#include "win/stat_window.hpp"
#include "win/pad_assign.hpp"
#include "win/pad_tiles.hpp"
#include "win/party_panel.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/reference_window.hpp"

namespace uapad::runtime {
namespace {

struct State {
    bool initDone = false;
    bool failed = false;
    DWORD gameThread = 0;
    bool warnedThread = false;
    HWND hwnd = nullptr;
    Config cfg;
    std::unique_ptr<Session> session;
    Pads pad;  // 0.6: Xbox or Sony
    bool npcSupported = false;  // 0.7: the NPC windows are read and answered in the client's memory
    std::string shownHint;
    VirtualMouse mouse;        // what the client currently gets from us
    VirtualButton button;      // virtual LMB as the client has been told about it
    bool loggedNullRead = false;
    CursorBridge bridge;     // drives the client's own cursor (it only reads DirectInput deltas)
    bool lastFocused = false;
    int loggedSteps = 0;
    int pointersHooked = 0;
    DWORD lastPointerCheckMs = 0;
    bool checkedPointers = false;
    bool loggedBufferRead = false;
    bool loggedMark = false;
    SettingsMenu menu;          // behind Start
    bool swallowMouse = false;  // mouse buttons are the menu's: while it is open and until let go after
    uint16_t prevPadButtons = 0;
    std::string bindsPath;      // uaro-pad-binds.ini
    std::string iniPath;        // uaro-pad.ini
    int sleepWhenOn = 10;       // background_sleep_ms to write when the menu switches the CPU saving on
    Settings menuBefore;        // the settings as the menu found them: only what changed gets written
    bool mouseSupported = false;  // the client's cursor and resolution can be read from its memory
    bool loggedExact = false;
    MouseGate gate;             // the real mouse's clicks and wheel reach the game only over it
    int claimsLogged = 0;       // 3.0.11-beta.33: «a press a part of the driver's took» lines
    MouseKeys mouseKeys;        // the middle and side buttons as F1-F9
    bool loggedLock = false;
    bool cameraSupported = false;  // the client's camera can be read and turned in its memory
    bool loggedCameraFail = false;
    int pendingZoom = 0;           // wheel notches the right stick asked for, one per mouse poll (> 0: closer)
    bool barSupported = false;  // the client's hotkey bar can be read from its memory
    bool loggedBarFail = false;
    DWORD lastBarMs = 0;
    bool barRead = false;
    bool realLmb = false;      // the real left button as DirectInput last reported it
    bool realClicked = false;  // it went down since the last tick
    bool loggedConnected = false;
    uint16_t loggedButtons = 0;
    bool ticked = false;
    DWORD lastTickMs = 0;
    // 3.0.11-beta.86: what the player plays with now (core/input_mode) - the pad's presses and sticks against the real
    // mouse's presses and moves (adding up while it keeps moving) and the keys
    uagfx::inputmode::Mode mode;
    uint16_t modeButtons = 0;
    bool padConnected = false;
    ULONGLONG travelAt = 0;
    int travelX = 0, travelY = 0;
    // 3.0.11-beta.88: walking without the mouse - the map's walkable cells (the reference's walkable.bin)
    bool directWalk = false;
    bool tiles = true;          // 3.0.11-beta.90: [interface] pad_tiles
    bool ownHints = true;       // 3.0.11-beta.92: the hint line drawn by the interface (win/pad_hint)
    std::vector<HintPart> hintParts;
    WindowOrder windows;        // 3.0.11-beta.93: the windows B closes, in the order they opened
    bool backDown = false;      // B held at the last tick
    bool backHeld = false;      // the press that closed a window: not the game's until it is let go
    bool openMenuNow = false;   // the tile's «Контролер» chosen: the pad's menu opens at the next tick
    std::string groundMap;
    GroundGrid ground;
    DWORD groundTriedMs = 0;
    unsigned long nState = 0, nData = 0, nCursor = 0, nPump = 0, nRecv = 0, nSend = 0;
    DWORD lastDiagMs = 0;
};

State& S() {
    static State s;
    return s;
}

constexpr LONG kWheelNotch = 120;

// Hooks running off the game thread only record that the map socket carried traffic we could not
// parse; the game thread turns that into Session::onUnobserved on its next tick.
std::atomic<uintptr_t> g_mapSocket{0};
std::atomic<bool> g_unseenUp{false};
std::atomic<bool> g_unseenDown{false};

void noteUnseen(SOCKET sock, bool up) {
    if (uintptr_t(sock) != g_mapSocket.load()) return;
    (up ? g_unseenUp : g_unseenDown).store(true);
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// The file's bytes; "" when there is no such file; nothing when it is there but cannot be read now (a virus
// scanner or an editor holding it). Saving over an unread file would replace the player's file with a stub.
std::optional<std::string> readExisting(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (f) {
        std::ostringstream ss;
        ss << f.rdbuf();
        if (f.bad()) return std::nullopt;
        return ss.str();
    }
    const DWORD attrs = GetFileAttributesA(path.c_str());
    const DWORD err = GetLastError();
    if (attrs == INVALID_FILE_ATTRIBUTES && (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND))
        return std::string();
    return std::nullopt;
}

bool onGameThread() {
    State& s = S();
    if (GetCurrentThreadId() == s.gameThread) return true;
    if (!s.warnedThread) {
        s.warnedThread = true;
        LOG_INFO("a hook was called from a non-game thread (%lu); such calls are ignored",
                 (unsigned long)GetCurrentThreadId());
    }
    return false;
}

void fail(const char* where) {
    State& s = S();
    if (!s.failed) LOG_ERROR("internal error in %s; pass-through until the client restarts", where);
    s.failed = true;
    // Never leave the client with a held virtual button: queue a release (handed out by onMouseData
    // even after the failure) or, for the message channel, post it right away.
    if (s.button.release(GetTickCount()) && s.cfg.buttons == ButtonChannel::Messages && s.hwnd)
        PostMessageA(s.hwnd, WM_LBUTTONUP, 0, MAKELPARAM(s.mouse.x, s.mouse.y));
    s.mouse = VirtualMouse{};
    s.pad.stopRumble();
    s.menu.close();
    overlay::hide();
    gamewindow::releaseCursor();
    for (const MouseKeys::KeyEvent& e : s.mouseKeys.releaseAll()) gamewindow::sendFKey(e.key, false);
}

template <class F>
void guarded(const char* where, F&& body) {
    State& s = S();
    if (!s.initDone || s.failed) return;
    try {
        body();
    } catch (const std::exception& e) {
        LOG_ERROR("%s: %s", where, e.what());
        fail(where);
    } catch (...) {
        fail(where);
    }
}

ViewInfo currentView() {
    State& s = S();
    ViewInfo v;
    if (!s.hwnd) return v;
    v.focused = GetForegroundWindow() == s.hwnd;
    v.mouseClicked = s.realClicked;
    s.realClicked = false;
    RECT r;
    if (GetClientRect(s.hwnd, &r)) {
        v.width = r.right - r.left;
        v.height = r.bottom - r.top;
    }
    return v;
}

void applyMouse(const VirtualMouse& m) {
    State& s = S();
    if (m.active != s.mouse.active && s.hwnd) {
        POINT pt{};
        GetCursorPos(&pt);
        ScreenToClient(s.hwnd, &pt);
        if (m.active)
            LOG_INFO("virtual mouse on at (%d,%d), button %s", m.x, m.y, m.lmb ? "down" : "up");
        else
            LOG_INFO("virtual mouse off at (%d,%d); the game cursor goes back to the mouse at (%ld,%ld)", s.mouse.x,
                     s.mouse.y, pt.x, pt.y);
    }
    const bool lmb = m.active && m.lmb;
    if (s.button.set(lmb, GetTickCount())) {
        if (s.cfg.buttons == ButtonChannel::Messages && s.hwnd)
            PostMessageA(s.hwnd, lmb ? WM_LBUTTONDOWN : WM_LBUTTONUP, lmb ? MK_LBUTTON : 0, MAKELPARAM(m.x, m.y));
    } else if (lmb && s.cfg.buttons == ButtonChannel::Messages && s.hwnd && (m.x != s.mouse.x || m.y != s.mouse.y)) {
        PostMessageA(s.hwnd, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(m.x, m.y));
    }
    s.mouse = m;
}

void flushPackets() {
    State& s = S();
    std::vector<Bytes> ready = s.session->takeInjectable(GetTickCount());
    if (ready.empty()) return;
    const SOCKET sock = SOCKET(s.session->mapSocket());
    for (const Bytes& p : ready) {
        if (!hooks::sendAll(sock, p.data(), int(p.size()))) {
            LOG_ERROR("sending a pad request failed (WSA error %d)", hooks::lastError());
            s.session->markInjectBroken();
            return;
        }
        LOG_DEBUG("sent 0x%04x (%u bytes)", unsigned(rd16(p.data(), 0)), unsigned(p.size()));
    }
}

void logDiagnostics(DWORD now) {
    State& s = S();
    if (s.lastDiagMs != 0 && now - s.lastDiagMs < 10000) return;
    s.lastDiagMs = now;
    LOG_INFO("diag: mouse GetDeviceState=%lu GetDeviceData=%lu | GetCursorPos=%lu | PeekMessageA=%lu | recv=%lu "
             "send=%lu | map=%s self=%lu",
             s.nState, s.nData, s.nCursor, s.nPump, s.nRecv, s.nSend, s.session->mapSocket() ? "yes" : "no",
             (unsigned long)s.session->world().selfId());
}

bool writeAtomically(const std::string& path, const std::string& text);

void logPad(const PadState& p) {
    State& s = S();
    if (p.connected != s.loggedConnected) {
        s.loggedConnected = p.connected;
        LOG_INFO("gamepad %s", p.connected ? "connected" : "disconnected");
        if (p.connected) {  // 3.0.11-beta.89: the app's page «Контролер» says which one the game saw last
            const std::string line = std::to_string(static_cast<long long>(std::time(nullptr))) + "\t" + s.pad.name();
            writeAtomically(uagfx::layout::join(uagfx::win::gameDir(), uagfx::layout::kPadSeen), line + "\n");
            LOG_INFO("gamepad: %s", s.pad.name());
        }
    }
    if (p.buttons != s.loggedButtons) {
        s.loggedButtons = p.buttons;
        LOG_DEBUG("buttons 0x%04x", unsigned(p.buttons));
    }
}

std::wstring widen(const std::string& text, UINT codepage) {
    if (text.empty()) return std::wstring();
    const int n = MultiByteToWideChar(codepage, 0, text.data(), int(text.size()), nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring wide(size_t(n), L'\0');
    MultiByteToWideChar(codepage, 0, text.data(), int(text.size()), &wide[0], n);
    return wide;
}

overlay::View menuView() {
    State& s = S();
    const Lang lang = s.menu.lang();
    overlay::View v;
    const uint32_t id = s.session->charId();
    const std::string& name = s.session->charName();  // the server's spelling: the system code page
    v.title = L"uaRO Pad — " + (id == 0          ? widen(tr(lang, Text::DefaultLayout), CP_UTF8)
                                : !name.empty() ? widen(name, CP_ACP)
                                                : widen(tr(lang, Text::Character), CP_UTF8) + L" " +
                                                      std::to_wstring(id));
    v.tabs = {widen(s.menu.tabTitle(SettingsMenu::Tab::Buttons), CP_UTF8),
              widen(s.menu.tabTitle(SettingsMenu::Tab::Settings), CP_UTF8)};
    v.tab = size_t(s.menu.tab());
    for (const SettingsMenu::Row& row : s.menu.rows())
        v.rows.push_back(overlay::View::Row{widen(row.label, CP_UTF8), widen(row.value, CP_UTF8), !row.hasValue});
    v.selected = s.menu.selected();
    v.rowSlots = SettingsMenu::maxRowCount();
    v.labelWidth = s.menu.tab() == SettingsMenu::Tab::Settings ? 286 : 166;  // setting names are long
    v.help = widen(tr(lang, Text::Help), CP_UTF8);
    v.save = widen(tr(lang, Text::Save), CP_UTF8);
    v.cancel = widen(tr(lang, Text::Cancel), CP_UTF8);
    return v;
}

// 3.0.11-beta.89 (the concept «Контроллер в интерфейсе uaRO»: one program, one language): the pad's menu and hints in
// the language of uaRO Graphics' interface, whatever uaro-pad.ini says
void followDriverLanguage(Config& cfg) { cfg.lang = Lang(int(uagfx::win::startup().lang)); }

// Writes `text` to `path` through a temporary file, so a crash never leaves half a file behind.
bool writeAtomically(const std::string& path, const std::string& text) {
    const std::string tmp = path + ".tmp";
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    f << text;
    f.close();
    if (!f || !MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        LOG_ERROR("could not save %s (error %lu)", path.c_str(), (unsigned long)GetLastError());
        DeleteFileA(tmp.c_str());
        return false;
    }
    return true;
}

// The Settings tab saved `after`: write what changed into uaro-pad.ini (read again first: it may have been
// edited meanwhile) and let it work now, written or not.
bool saveSettings(const Settings& after) {
    State& s = S();
    const std::vector<IniEdit> edits = settingsEdits(s.menuBefore, after, s.sleepWhenOn);
    if (edits.empty()) return true;
    std::optional<std::string> onDisk = readExisting(s.iniPath);
    // 2.9: on by default, no file yet - the first save starts from the package's template, with its notes
    if (onDisk && onDisk->empty())
        onDisk = readExisting(uagfx::layout::join(uagfx::win::gameDir(), uagfx::layout::kPadTemplate));
    if (!onDisk)
        LOG_ERROR("could not read %s; settings not saved, they work until the client closes", s.iniPath.c_str());
    const bool written = onDisk && writeAtomically(s.iniPath, applyIniEdits(*onDisk, edits));
    if (written) LOG_INFO("uaro-pad.ini: %u setting(s) saved", unsigned(edits.size()));
    const Config old = s.cfg;
    applySettings(after, s.sleepWhenOn, s.cfg);
    followDriverLanguage(s.cfg);
    if (s.cfg.backgroundSleepMs > 0) s.sleepWhenOn = s.cfg.backgroundSleepMs;
    s.session->setMark(s.cfg.mark);
    s.session->setWalkRadius(s.cfg.walkRadius);
    if (old.borderless != s.cfg.borderless) {
        const bool ok = gamewindow::setBorderless(s.hwnd, s.cfg.borderless, s.cfg.position);
        LOG_INFO("borderless window %s%s", s.cfg.borderless ? "on" : "off", ok ? "" : ": not possible now");
    }
    LOG_INFO("settings: language %s, cursor lock %d, activating click %s, mouse keys %s/%s/%s, CPU sleep %d ms, "
             "rumble %d, mark %s",
             langToken(s.cfg.lang), s.cfg.mouseLock ? 1 : 0, s.cfg.focusClick ? "to the game" : "kept out",
             fKeyToken(s.cfg.mouseKeys[0]).c_str(), fKeyToken(s.cfg.mouseKeys[1]).c_str(),
             fKeyToken(s.cfg.mouseKeys[2]).c_str(), s.cfg.backgroundSleepMs, s.cfg.rumble ? 1 : 0,
             markStyleToken(s.cfg.mark));
    return written;
}

// Writes this character's layout into uaro-pad-binds.ini. The file is read again first: another client may
// have saved its own character meanwhile, and that must survive.
bool saveLayouts() {
    State& s = S();
    const uint32_t id = s.session->charId();
    const Profiles& mine = s.session->profiles();
    const Bindings saved = id ? mine.forCharacter(id) : mine.fallback;
    const std::optional<std::string> onDisk = readExisting(s.bindsPath);
    if (!onDisk) {
        LOG_ERROR("could not read %s; not saved, the layout works until the client closes", s.bindsPath.c_str());
        return false;
    }
    const Profiles merged = withLayout(parseProfiles(*onDisk), id, s.session->charName(), saved);
    // the layout works until the client closes
    if (!writeAtomically(s.bindsPath, formatProfiles(merged))) return false;
    s.session->setProfiles(merged);  // with whatever other clients saved
    return true;
}

// The settings menu behind Start. True while the menu had the pad this tick (the tick it closes included,
// so the button that closed it does not reach the game as a new press).
// 3.0.11-beta.21: «Довідник» (uaRO Graphics' reference window) open - the pad's buttons are its: the D-pad picks a
// row, A opens it, B goes back, Start closes it, the bumpers scroll; the character waits meanwhile
bool referencePad(const PadState& pad) {
    State& s = S();
    const uint16_t buttons = pad.connected ? pad.buttons : 0;
    const uint16_t down = uint16_t(buttons & ~s.prevPadButtons);
    s.prevPadButtons = buttons;
    using uagfx::win::referencewin::PadCommand;
    const std::pair<uint16_t, PadCommand> keys[] = {
        {kPadUp, PadCommand::Up},       {kPadDown, PadCommand::Down},     {kPadA, PadCommand::Enter},
        {kPadB, PadCommand::Back},      {kPadStart, PadCommand::Close},   {kPadLB, PadCommand::PageUp},
        {kPadRB, PadCommand::PageDown}};
    for (const auto& [button, command] : keys)
        if (down & button) uagfx::win::referencewin::padCommand(command);
    return true;
}

// 3.0.11-beta.90 (the concept's stage 6): the windows' tile open - the pad's buttons are its: the D-pad (or the left
// stick) moves its frame, A opens, B or Start closes; the character waits meanwhile
uint16_t stickSteps(const PadState& pad, DWORD now);

bool tilePad(const PadState& pad, DWORD now) {
    State& s = S();
    const uint16_t buttons = pad.connected ? pad.buttons : 0;
    const uint16_t down = uint16_t((buttons & ~s.prevPadButtons) | stickSteps(pad, now));
    s.prevPadButtons = buttons;
    using uagfx::win::padtileswin::Command;
    const std::pair<uint16_t, Command> keys[] = {{kPadUp, Command::Up},       {kPadDown, Command::Down},
                                                 {kPadLeft, Command::Left},   {kPadRight, Command::Right},
                                                 {kPadA, Command::Enter},     {kPadB, Command::Close},
                                                 {kPadStart, Command::Close}};
    for (const auto& [button, command] : keys)
        if (down & button) uagfx::win::padtileswin::command(command);
    return true;
}

// The left stick as the D-pad for the windows: a step on a push, again every 250 ms held; 0 none
uint16_t stickSteps(const PadState& pad, DWORD now) {
    static DWORD stickAt = 0;
    const bool pushed = std::fabs(pad.lx) > 0.6f || std::fabs(pad.ly) > 0.6f;
    if (!pushed) {
        stickAt = 0;
        return 0;
    }
    if (stickAt && now - stickAt < 250) return 0;
    stickAt = now ? now : 1;
    if (std::fabs(pad.lx) > std::fabs(pad.ly)) return pad.lx > 0 ? kPadRight : kPadLeft;
    return pad.ly > 0 ? kPadUp : kPadDown;
}

// 3.0.11-beta.91 (the concept's stage 6): «Призначити» open - the D-pad chooses the button, A puts, B or Start cancel
bool assignPad(const PadState& pad, DWORD now) {
    State& s = S();
    const uint16_t buttons = pad.connected ? pad.buttons : 0;
    const uint16_t down = uint16_t((buttons & ~s.prevPadButtons) | stickSteps(pad, now));
    s.prevPadButtons = buttons;
    using uagfx::win::padassignwin::Command;
    const std::pair<uint16_t, Command> keys[] = {{kPadUp, Command::Up},   {kPadDown, Command::Down},
                                                 {kPadA, Command::Enter}, {kPadB, Command::Close},
                                                 {kPadStart, Command::Close}};
    for (const auto& [button, command] : keys)
        if (down & button) uagfx::win::padassignwin::command(command);
    return true;
}

// 3.0.11-beta.91: «Речі» open - the D-pad (or the left stick) moves its frame, A uses, Y its card, X «Призначити»,
// LB / RB the tabs, B or Start close the game's window; the character stands meanwhile
bool itemsPad(const PadState& pad, DWORD now) {
    State& s = S();
    const uint16_t buttons = pad.connected ? pad.buttons : 0;
    const uint16_t down = uint16_t((buttons & ~s.prevPadButtons) | stickSteps(pad, now));
    s.prevPadButtons = buttons;
    using uagfx::win::itemswin::PadCommand;
    const std::pair<uint16_t, PadCommand> keys[] = {
        {kPadUp, PadCommand::Up},     {kPadDown, PadCommand::Down}, {kPadLeft, PadCommand::Left},
        {kPadRight, PadCommand::Right}, {kPadA, PadCommand::Use},   {kPadY, PadCommand::Card},
        {kPadX, PadCommand::Assign},  {kPadLB, PadCommand::TabBack}, {kPadRB, PadCommand::TabOn},
        {kPadB, PadCommand::Close},   {kPadStart, PadCommand::Close}};
    for (const auto& [button, command] : keys)
        if (down & button) uagfx::win::itemswin::padCommand(command);
    return true;
}

// 3.0.11-beta.92: «Характеристики» open - the D-pad (or the left stick) chooses a stat, A adds a point, B or Start
// close it
bool statsPad(const PadState& pad, DWORD now) {
    State& s = S();
    const uint16_t buttons = pad.connected ? pad.buttons : 0;
    const uint16_t down = uint16_t((buttons & ~s.prevPadButtons) | stickSteps(pad, now));
    s.prevPadButtons = buttons;
    using uagfx::win::statswin::PadCommand;
    const std::pair<uint16_t, PadCommand> keys[] = {{kPadUp, PadCommand::Up},    {kPadDown, PadCommand::Down},
                                                    {kPadA, PadCommand::Raise},  {kPadB, PadCommand::Close},
                                                    {kPadStart, PadCommand::Close}};
    for (const auto& [button, command] : keys)
        if (down & button) uagfx::win::statswin::padCommand(command);
    return true;
}

bool runMenu(const PadState& pad, const ViewInfo& view, DWORD now) {
    State& s = S();
    const uint16_t buttons = pad.connected ? pad.buttons : 0;
    const uint16_t down = uint16_t(buttons & ~s.prevPadButtons);
    s.prevPadButtons = buttons;
    if (!s.menu.isOpen()) {
        // 3.0.11-beta.85: Start while a support skill asks whom cancels that, it opens no menu
        const bool start = (down & kPadStart) && view.focused && s.hwnd && s.session->controller().choosing() == 0;
        if (start && s.tiles && !s.openMenuNow) {  // 3.0.11-beta.90: the windows' tile instead
            uagfx::win::padtileswin::open();
            return true;
        }
        if (!start && !(s.openMenuNow && view.focused && s.hwnd)) return false;
        s.openMenuNow = false;
        s.menuBefore = settingsFrom(s.cfg);
        s.menu.open(s.session->controller().bindings(), s.menuBefore, s.session->world().clientBarNames(), buttons,
                    now, pad.lx, pad.ly);
        s.session->askOwnName(now);
        LOG_INFO("settings menu opened");
    }
    SettingsMenu::Result result = view.focused ? s.menu.update(pad, now) : SettingsMenu::Result::Cancel;
    for (const overlay::MouseEvent& e : overlay::takeClicks()) {
        switch (e.click) {
            case overlay::Click::Tab:
                s.menu.showTab(e.row == 0 ? SettingsMenu::Tab::Buttons : SettingsMenu::Tab::Settings);
                break;
            case overlay::Click::Select:
                s.menu.select(e.row);
                if (e.row == s.menu.rowCount() - 1) s.menu.resetTab();
                break;
            case overlay::Click::Previous:
                s.menu.change(e.row, -1);
                break;
            case overlay::Click::Next:
                s.menu.change(e.row, +1);
                break;
            case overlay::Click::Save:
                result = SettingsMenu::Result::Save;
                break;
            case overlay::Click::Cancel:
                result = SettingsMenu::Result::Cancel;
                break;
        }
    }
    if (result == SettingsMenu::Result::Save) {
        s.session->saveBindings(s.menu.bindings());
        const bool layouts = saveLayouts();
        const bool settings = saveSettings(s.menu.settings());
        if ((!layouts || !settings) && s.cfg.rumble) s.pad.rumble(Rumble::Denied, now);
    }
    if (result != SettingsMenu::Result::None) {
        s.menu.close();
        overlay::hide();
        LOG_INFO(result == SettingsMenu::Result::Save ? "settings saved" : "settings closed without saving");
        return true;
    }
    if (!overlay::show(s.hwnd, menuView())) {
        s.menu.close();
        LOG_ERROR("could not create the settings window");
    }
    return true;
}

// The game window coming to the front or going behind; the middle and side mouse buttons as F-keys; the
// cursor kept inside the window.
void followTheMouse(bool foreground, DWORD now) {
    State& s = S();
    s.gate.watch(foreground, now);  // the ticks see the game go behind: its mouse polls stop then
    if (!s.hwnd) return;
    const bool keysWanted = s.cfg.mouseKeys[0] || s.cfg.mouseKeys[1] || s.cfg.mouseKeys[2];
    uint8_t held = 0;
    if (keysWanted) {
        if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) held |= 1;
        if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) held |= 2;
        if (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) held |= 4;
    }
    POINT cursor{};
    GetCursorPos(&cursor);
    const bool allowed = foreground && !s.menu.isOpen() && gamewindow::cursorOverGame(s.hwnd, cursor);
    for (const MouseKeys::KeyEvent& e : s.mouseKeys.update(held, s.cfg.mouseKeys, allowed, foreground)) {
        gamewindow::sendFKey(e.key, e.down);
        LOG_DEBUG("mouse button: F%d %s", e.key, e.down ? "down" : "up");
    }
    const bool lock = s.cfg.mouseLock && s.cfg.mouseFreedom && foreground && !IsIconic(s.hwnd);
    gamewindow::lockCursor(s.hwnd, lock);
    if (lock != s.loggedLock) {
        s.loggedLock = lock;
        LOG_INFO(lock ? "cursor kept inside the game window" : "cursor free to leave the game window");
    }
}

// 3.0.11-beta.88 (the concept's stage 4): walking without the mouse - which way the camera looks along the ground (the
// view's forward, as the minimap reads it) and the map's walkable cells, for the controller to find the cell to ask for
void lookAndGround(DWORD now) {
    State& s = S();
    Controller& control = s.session->control();
    control.setDirectWalk(s.directWalk);
    if (!s.directWalk) return;
    float view[12];
    const float len = uagfx::win::readGameCamera(view) ? std::hypot(view[2], view[8]) : 0.0f;
    if (len > 1e-3f)
        control.setLook(double(view[2] / len), double(view[8] / len));
    else
        control.clearLook();
    const std::string map = uagfx::win::readGameMap();
    if (map != s.groundMap || (!s.ground.known() && !map.empty() && now - s.groundTriedMs >= 2000)) {
        const bool fresh = map != s.groundMap;
        s.groundMap = map;
        s.groundTriedMs = now;
        s.ground = GroundGrid{};
        const auto index = uagfx::win::reference::index();
        if (index && index->walk) {
            const auto it = index->walk->find(map);
            if (it != index->walk->end()) {
                const uagfx::walkpath::Cells cells = uagfx::walkpath::cellsOf(it->second);
                s.ground.w = cells.w;
                s.ground.h = cells.h;
                s.ground.walk = cells.walk;
            }
        }
        if (fresh || s.ground.known())
            LOG_INFO("walk without the mouse: %s - %s", map.c_str(),
                     s.ground.known() ? "its walkable cells read" : "no walkable cells known yet: any cell asked for");
    }
    control.setGround(&s.ground);
}

// 0.7: what the pad asked of the NPC window, and the line that says what its buttons do there (3.0.11-beta.85: or
// while a support skill asks whom).
void answerNpc(const TickOutput& out, const ViewInfo& view, const PadState& pad) {
    State& s = S();
    for (DialogCommand c : out.dialog)
        if (!clientnpc::answer(c)) LOG_INFO("NPC window: command %d found no window to answer", int(c));
    if (out.number) clientnpc::typeNumber(s.hwnd, *out.number);
    const bool answering = s.session->controller().dialog() != NpcDialog::None && view.dialog != NpcDialog::None;
    const uint16_t choiceKey = s.session->controller().choosingButton();  // 3.0.11-beta.85: a support skill asks whom
    const bool shown = pad.connected && view.focused && !view.paused;
    // 3.0.11-beta.93: a window B closes open while the player plays with the pad
    const bool closing = s.windows.last() >= 0 && s.mode.padShown(GetTickCount64());
    const std::string text = !shown            ? std::string()
                             : choiceKey != 0  ? choiceHint(choiceKey, s.cfg.lang)
                             : s.cfg.npcHint && answering ? npcHint(view.dialog, s.cfg.lang)
                             : closing && view.dialog == NpcDialog::None ? windowHint(s.cfg.lang)
                                                          : std::string();
    s.hintParts = text.empty()     ? std::vector<HintPart>{}  // 3.0.11-beta.92: for the interface's own line
                  : choiceKey != 0 ? choiceHintParts(choiceKey, s.cfg.lang)
                  : s.cfg.npcHint && answering ? npcHintParts(view.dialog, s.cfg.lang)
                                               : windowHintParts(s.cfg.lang);
    if (s.ownHints) {  // drawn by the interface: the pad's own window away
        if (!s.shownHint.empty()) hint::hide();
        s.shownHint.clear();
        return;
    }
    if (text == s.shownHint) return;
    s.shownHint = text;
    if (text.empty())
        hint::hide();
    else
        hint::show(s.hwnd, widen(text, CP_UTF8));
}

// 3.0.11-beta.93 (R-KMG8GE: B pressed on the game's equipment and options windows and on the big map went to the
// empty slot F3; the big map closed only with the mouse): while none of the pad's own windows has the buttons, B
// closes the window opened last of the big map and the game's windows the tile opens - unless an NPC window or a
// skill's «whom?» has B. That press goes no further: B's own action waits for the next one.
struct Closable {
    const char* name;
    int command;  // the game's Basic Info button opening it; 0 the big map
};
constexpr Closable kClosable[] = {{"the big map", 0},
                                  {"the game's equipment", 0x81},
                                  {"the game's skills", 0x82},
                                  {"the game's options", 0x7F},
                                  {"the game's party and friends", 0x85}};

bool closableOpen(const Closable& w) {
    return w.command == 0 ? uagfx::win::minimapproto::bigShown() : uagfx::win::charpanel::gameOpen(w.command);
}

void backCloses(PadState& pad, const ViewInfo& view) {
    State& s = S();
    for (size_t i = 0; i < std::size(kClosable); ++i) s.windows.see(int(i), closableOpen(kClosable[i]));
    const bool b = pad.connected && (pad.buttons & kPadB);
    const bool down = b && !s.backDown;
    s.backDown = b;
    if (s.backHeld) {
        if (b)
            pad.buttons = uint16_t(pad.buttons & ~kPadB);
        else
            s.backHeld = false;
        return;
    }
    const int last = s.windows.last();
    if (!down || view.paused || last < 0 || !view.focused || view.dialog != NpcDialog::None ||
        s.session->controller().choosing() != 0)
        return;
    const Closable& w = kClosable[last];
    if (w.command == 0)
        uagfx::win::minimapproto::showBig(false);
    else
        uagfx::win::charpanel::pressGame(w.command);
    const bool closed = !closableOpen(w);
    s.windows.see(last, !closed);
    LOG_INFO("B: %s %s", w.name, closed ? "closed" : "still open - the game did not close it");
    s.backHeld = true;
    pad.buttons = uint16_t(pad.buttons & ~kPadB);
}

void tick() {
    State& s = S();
    const DWORD now = GetTickCount();
    if (s.ticked && now - s.lastTickMs < 5) return;  // the client may poll several times per frame
    s.ticked = true;
    s.lastTickMs = now;
    if (g_unseenUp.exchange(false)) s.session->onUnobserved(s.session->mapSocket(), Direction::ClientToServer);
    if (g_unseenDown.exchange(false)) s.session->onUnobserved(s.session->mapSocket(), Direction::ServerToClient);
    const PadState pad = s.pad.poll();
    logPad(pad);
    if (pad.connected) {  // 3.0.11-beta.86: an intentional pad input shows its buttons on the interface
        const uint16_t pressed = uint16_t((pad.buttons & ~s.modeButtons) | (pad.lt > 0.5f || pad.rt > 0.5f ? 1 : 0));
        if (uagfx::inputmode::intentional(pressed, pad.lx, pad.ly, pad.rx, pad.ry)) s.mode.pad(GetTickCount64());
    }
    s.modeButtons = pad.connected ? pad.buttons : 0;
    s.padConnected = pad.connected;
    // The client fills its send/recv pointers itself (and may refill them): keep them going through us.
    if (!s.checkedPointers || now - s.lastPointerCheckMs >= (s.pointersHooked >= 2 ? 2000u : 250u)) {
        s.checkedPointers = true;
        s.lastPointerCheckMs = now;
        s.pointersHooked = hooks::patchClientPointers();
    }
    // The client never sends its hotkey bar: look at it in its memory now and then.
    if (s.barSupported && (!s.barRead || now - s.lastBarMs >= 250)) {
        s.barRead = true;
        s.lastBarMs = now;
        std::array<ClientBarSlot, kClientBarSlots> bar;
        if (clientbar::read(bar)) {
            s.session->setClientBar(bar);
        } else if (!s.loggedBarFail) {
            s.loggedBarFail = true;
            LOG_INFO("could not read the client's hotkey bar this time");
        }
    }
    ViewInfo view = currentView();
    if (view.focused != s.lastFocused) {  // the real mouse may have moved the client's cursor meanwhile
        s.lastFocused = view.focused;
        s.bridge.reset();
    }
    followTheMouse(view.focused, now);
    // 3.0.11-beta.21: the reference window's while it is open, else the pad's own menu (Start)
    const bool menuOpen = S().menu.isOpen();
    view.paused = uagfx::win::referencewin::isOpen() && !menuOpen     ? referencePad(pad)
                  : uagfx::win::padassignwin::isOpen() && !menuOpen ? assignPad(pad, now)  // 3.0.11-beta.91
                  : uagfx::win::padtileswin::isOpen() && !menuOpen  ? tilePad(pad, now)    // 3.0.11-beta.90
                  : uagfx::win::itemswin::shown() && !menuOpen && pad.connected ? itemsPad(pad, now)  // .91
                  : uagfx::win::statswin::shown() && !menuOpen && pad.connected ? statsPad(pad, now)  // .92
                                                                               : runMenu(pad, view, now);
    if (s.npcSupported) view.dialog = clientnpc::read();
    double clientTurn = 0;
    if (s.cameraSupported && clientcamera::readTurn(clientTurn))
        s.session->setClientTurn(clientTurn * 3.14159265358979323846 / 180.0);
    else
        s.session->setClientTurn(std::nullopt);
    s.session->setFriends(uagfx::win::partywin::hereIds());  // 3.0.11-beta.85: whom a support skill may go on
    lookAndGround(now);  // 3.0.11-beta.88
    PadState padIn = pad;
    backCloses(padIn, view);  // 3.0.11-beta.93: each tick, so the press closing the tile closes nothing more
    const TickOutput out = s.session->tick(padIn, view, now);
    if (out.camera.turnDeg != 0 && s.cameraSupported && !clientcamera::turn(out.camera.turnDeg) &&
        !s.loggedCameraFail) {
        s.loggedCameraFail = true;
        LOG_INFO("camera: could not turn it now (not in the game?)");
    }
    s.pendingZoom = std::clamp(s.pendingZoom + out.camera.zoomSteps, -8, 8);
    if (!view.focused) s.pendingZoom = 0;
    for (const std::string& note : out.notes) LOG_INFO("%s", note.c_str());
    answerNpc(out, view, pad);
    if (out.reference) uagfx::win::referencewin::toggleFromPad();  // 3.0.11-beta.22: a button bound to it
    for (const std::string& line : s.session->takeLog()) LOG_INFO("%s", line.c_str());
    if (s.cfg.rumble && out.rumble != Rumble::None) s.pad.rumble(out.rumble, now);
    s.pad.update(now);
    applyMouse(out.mouse);
    flushPackets();
    g_mapSocket.store(s.session->mapSocket());
    logDiagnostics(now);
}

}  // namespace

bool healthy() { return S().initDone && !S().failed; }

uint32_t choosing() { return healthy() && S().session ? S().session->controller().choosing() : 0; }

Look look() {
    Look l;
    State& s = S();
    if (!healthy() || !s.session) return l;
    l.shown = s.padConnected && s.session->controller().enabled() && s.mode.padShown(GetTickCount64());
    l.sony = s.pad.sony();
    l.bindings = s.session->controller().bindings();
    return l;
}

uint32_t target() { return healthy() && S().session ? S().session->controller().target() : 0; }

uint32_t preview() { return healthy() && S().session ? S().session->controller().preview() : 0; }

void setOwnHints(bool on) {
    State& s = S();
    if (on == s.ownHints) return;
    s.ownHints = on;
    LOG_INFO("the controller's hints: %s", on ? "the interface's line" : "the pad's own window");
}

std::vector<HintPart> hints() { return healthy() ? S().hintParts : std::vector<HintPart>{}; }

void setTiles(bool on) {
    State& s = S();
    if (on == s.tiles) return;
    s.tiles = on;
    LOG_INFO("Start: %s", on ? "the windows' tile" : "the controller's own menu");
}

void openPadMenu() {
    S().openMenuNow = true;
    LOG_INFO("the controller's own menu asked for from the tile");
}

void setDirectWalk(bool on) {
    State& s = S();
    if (on == s.directWalk) return;
    s.directWalk = on;
    LOG_INFO("walk without the mouse: %s", on ? "on (an arrow on the ground)" : "off (the game's cursor held out)");
}

bool walkTarget(int& x, int& y) {
    if (!healthy() || !S().session) return false;
    const std::optional<Cell> at = S().session->controller().walkTarget();
    if (!at) return false;
    x = at->x;
    y = at->y;
    return true;
}

void noteKeysOrMouse(bool click) {
    State& s = S();
    if (click && s.mouse.active) return;  // the pad's own virtual click (the messages channel), not the player's
    s.mode.mouse(GetTickCount64());
}

void init() {
    State& s = S();
    if (s.initDone || s.failed) return;
    try {
        // 2.9: uaRO Pad inside uaRO Graphics - its settings, layouts and log in uaro-graphics\ (core/layout); a 2.x
        // file not moved yet (the driver moves them at its start) is read where it is
        namespace layout = uagfx::layout;
        const std::string game = uagfx::win::gameDir();
        // the game may make its DirectInput before its DirectDraw (the driver's own move): the move first, here too
        uagfx::win::WinDisk disk;
        const std::vector<std::string> moved = layout::migrate(game, disk);
        s.iniPath = uagfx::win::ourFile(game, layout::File::PadIni);
        s.bindsPath = uagfx::win::ourFile(game, layout::File::PadBinds);
        s.cfg = parseConfig(readFile(s.iniPath));
        followDriverLanguage(s.cfg);  // 3.0.11-beta.89: the pad speaks the interface's language
        uagfx::win::makeFolders(layout::join(game, layout::kLogs));
        wlog::open(layout::join(game, layout::now(layout::File::PadLog)), s.cfg.logLevel);
        LOG_INFO("uaRO Pad %s", version());
        for (const std::string& line : moved) LOG_INFO("%s", line.c_str());
        s.gameThread = GetCurrentThreadId();
        s.session = std::make_unique<Session>(s.cfg);
        if (!s.cfg.enabled) LOG_INFO("starts switched off ([general] enabled = 0); press Back to switch on");
        if (s.cfg.backgroundSleepMs > 0) s.sleepWhenOn = s.cfg.backgroundSleepMs;
        const Profiles layouts = parseProfiles(readFile(s.bindsPath));
        s.session->setProfiles(layouts);
        LOG_INFO("layouts: %u character(s) in uaro-pad-binds.ini", unsigned(layouts.characters.size()));
        if (layouts.movedToCamera > 0)
            LOG_INFO("layouts from before 0.5: R3 now switches the right stick to the camera, an unused L3 clears "
                     "the target (%d layout(s))",
                     layouts.movedToCamera);
        if (layouts.movedToTalk > 0)
            LOG_INFO("layouts from before 0.7: D-pad down, on its default F9, now talks to the nearest NPC "
                     "(%d layout(s))",
                     layouts.movedToTalk);
        hooks::install();
        s.barSupported = clientbar::supported();
        s.mouseSupported = clientmouse::supported();
        s.cameraSupported = clientcamera::supported();
        s.npcSupported = clientnpc::supported();
        LOG_INFO(s.npcSupported ? "NPC windows: answered in the client's memory"
                                : "NPC windows: unknown client build; the pad does not answer them");
        LOG_INFO(s.cameraSupported ? "camera: turned in the client's memory"
                                   : "camera: unknown client build; the right stick only zooms");
        LOG_INFO(s.mouseSupported ? "game cursor: read from the client's memory"
                                  : "game cursor: unknown client build; its position is estimated");
        LOG_INFO(s.barSupported ? "hotkey bar: read from the client's memory"
                                : "hotkey bar: unknown client build; slots only from the server's 0x02b9 / 0x02ba");
        s.pad.configure(s.cfg);  // 0.6: [pad] type, [sony]
        if (s.pad.load())
            LOG_INFO("XInput: %s", s.pad.dllName());
        else
            LOG_INFO("XInput not found: gamepad unavailable");
        s.initDone = true;
    } catch (...) {
        s.failed = true;
        LOG_ERROR("init failed; pass-through only");
    }
}

void setGameWindow(HWND hwnd) {
    State& s = S();
    if (s.hwnd == hwnd) return;
    s.hwnd = hwnd;
    RECT w{}, c{};
    GetWindowRect(hwnd, &w);
    GetClientRect(hwnd, &c);
    LOG_INFO("game window %p: style 0x%08lx, window %ldx%ld at (%ld,%ld), client %ldx%ld", static_cast<void*>(hwnd),
             (unsigned long)GetWindowLongA(hwnd, GWL_STYLE), w.right - w.left, w.bottom - w.top, w.left, w.top,
             c.right - c.left, c.bottom - c.top);
    if (!healthy()) return;
    if (s.cfg.mouseFreedom) gamewindow::subclass(hwnd);  // the normal cursors over the title bar and borders
    if (s.cfg.borderless) {
        const bool ok = gamewindow::setBorderless(hwnd, true, s.cfg.position);
        RECT b{};
        GetWindowRect(hwnd, &b);
        LOG_INFO(ok ? "borderless window at (%ld,%ld), %ldx%ld" : "borderless window not possible (%ld,%ld %ldx%ld)",
                 b.left, b.top, b.right - b.left, b.bottom - b.top);
    }
}

void onMouseState(DWORD cb, LPVOID data) {
    State& s = S();
    if (s.nState++ == 0) LOG_INFO("first mouse GetDeviceState, %lu bytes", (unsigned long)cb);
    guarded("mouse state", [&] {
        if (!onGameThread()) return;
        tick();
        if (!data || cb < sizeof(DIMOUSESTATE) || !s.hwnd) return;
        auto* st = static_cast<DIMOUSESTATE*>(data);
        POINT screen{};
        GetCursorPos(&screen);
        uint8_t raw = 0;  // the device's own buttons, before anything of ours
        for (int i = 0; i < 4; ++i)
            if (st->rgbButtons[i] & 0x80) raw |= uint8_t(1u << i);
        {  // 3.0.11-beta.86: the real mouse - a press, or moves adding up past 8 px - brings the keys' names back
            const ULONGLONG at = GetTickCount64();
            if (at - s.travelAt > 300) s.travelX = s.travelY = 0;
            s.travelX += std::abs(int(st->lX));
            s.travelY += std::abs(int(st->lY));
            if (st->lX || st->lY) s.travelAt = at;
            if (raw || uagfx::inputmode::mouseMoved(s.travelX, s.travelY)) {
                s.mode.mouse(at);
                s.travelX = s.travelY = 0;
            }
        }
        // With mouse freedom DirectInput reports the mouse wherever the cursor is while the game is active:
        // only clicks and the wheel over the game window reach it, and not the click that activated it.
        // The open menu is not the game. The window's coming to the front is checked on every poll: the first
        // poll after a click on the window carries that click.
        const uint8_t took = gamewindow::takeClaimed();  // 3.0.11-beta.33: presses the driver's parts took
        if (s.cfg.mouseFreedom) {
            const DWORD now = GetTickCount();
            s.gate.watch(GetForegroundWindow() == s.hwnd, now);
            const bool over = !s.menu.isOpen() && gamewindow::cursorOverGame(s.hwnd, screen);
            if (took) {
                s.gate.claim(took);
                if ((raw & took) && over && s.claimsLogged < 3) {  // without it the game had it (the part gone)
                    ++s.claimsLogged;
                    LOG_INFO("mouse: a press a part of the driver's took (buttons 0x%x) kept from the game - the part "
                             "was gone before the game looked (its close button)", unsigned(raw & took));
                }
            }
            const MouseGate::Out pass = s.gate.filter(raw, over, s.cfg.focusClick, now);
            for (int i = 0; i < 4; ++i) st->rgbButtons[i] = (pass.buttons >> i) & 1 ? 0x80 : 0x00;
            if (!pass.wheel) st->lZ = 0;
        }
        if (s.pendingZoom != 0) {  // the right stick's zoom: one wheel notch a frame, the way the client counts it
            st->lZ = s.pendingZoom > 0 ? -kWheelNotch : kWheelNotch;  // the client moves closer on a negative wheel
            s.pendingZoom += s.pendingZoom > 0 ? -1 : 1;
        }
        const bool realLmb = (st->rgbButtons[0] & 0x80) != 0;  // a click here means the player took over
        // Clicks are the menu's while it is open, and so is the click that closed it until the button is let go.
        if (s.menu.isOpen())
            s.swallowMouse = true;
        else if (s.swallowMouse && raw == 0)
            s.swallowMouse = false;
        if (s.swallowMouse)
            std::memset(st->rgbButtons, 0, sizeof st->rgbButtons);
        else if (realLmb && !s.realLmb)
            s.realClicked = true;
        s.realLmb = realLmb;
        if (!s.cfg.mouseFreedom && !s.mouse.active) {
            s.bridge.reset();  // the real mouse moves the client's cursor now; resync before driving it again
            return;
        }
        // Target: the walking point while the stick is used, otherwise the Windows cursor (mouse freedom).
        POINT target{s.mouse.x, s.mouse.y};
        if (!s.mouse.active) {
            target = screen;
            ScreenToClient(s.hwnd, &target);
        }
        RECT rc{};
        GetClientRect(s.hwnd, &rc);
        const int w = rc.right - rc.left;
        const int h = rc.bottom - rc.top;
        int cx = 0, cy = 0, gw = 0, gh = 0;
        const bool exact = s.mouseSupported && clientmouse::read(cx, cy, gw, gh);
        if (exact && !s.loggedExact) {
            s.loggedExact = true;
            LOG_INFO("game cursor at (%d,%d) in a %dx%d game, window %dx%d", cx, cy, gw, gh, w, h);
        }
        const CursorBridge::Step step =
            exact ? s.bridge.stepExact(target.x, target.y, w, h, gw, gh, cx, cy)
                  : s.bridge.step(target.x, target.y, w, h);
        if (s.loggedSteps < 12 || step.settling) {
            ++s.loggedSteps;
            LOG_INFO("cursor: target (%ld,%ld) in %dx%d -> dx=%ld dy=%ld%s%s (raw dx=%ld dy=%ld)", target.x,
                     target.y, w, h, step.dx, step.dy, exact ? " exact" : "", step.settling ? " settling" : "",
                     st->lX, st->lY);
        }
        st->lX = step.dx;
        st->lY = step.dy;
        if (step.settling) {  // the cursor sits in the corner this frame: no clicks there
            st->rgbButtons[0] = 0;
            st->rgbButtons[1] = 0;
            return;
        }
        if (s.mouse.active && s.cfg.buttons == ButtonChannel::DirectInput)
            st->rgbButtons[0] = s.mouse.lmb ? 0x80 : 0x00;
    });
}

namespace {

// Adds our queued virtual button edges to a buffered DirectInput read.
void deliverButtons(DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD inOut, DWORD capacity, DWORD flags) {
    State& s = S();
    if (s.cfg.buttons != ButtonChannel::DirectInput || !inOut || cbObjectData < sizeof(DIDEVICEOBJECTDATA)) return;
    if (!rgdod) {  // count query (DIGDD_PEEK) or flush: our events count too
        *inOut += DWORD(s.button.pending());
        if (!(flags & DIGDD_PEEK)) s.button.flush();
        return;
    }
    const DWORD filled = *inOut;
    if (filled >= capacity) return;
    const std::vector<VirtualButton::Event> events = s.button.take(capacity - filled, (flags & DIGDD_PEEK) != 0);
    for (size_t i = 0; i < events.size(); ++i) {
        DIDEVICEOBJECTDATA ev{};
        ev.dwOfs = DIMOFS_BUTTON0;
        ev.dwData = events[i].down ? 0x80 : 0x00;
        ev.dwTimeStamp = events[i].timeMs;
        std::memcpy(reinterpret_cast<BYTE*>(rgdod) + (size_t(filled) + i) * cbObjectData, &ev, sizeof ev);
    }
    *inOut = filled + DWORD(events.size());
}

}  // namespace

void onMouseData(DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD inOut, DWORD capacity, DWORD flags) {
    State& s = S();
    ++s.nData;
    bool& logged = rgdod ? s.loggedBufferRead : s.loggedNullRead;
    if (!logged) {
        logged = true;
        LOG_INFO("mouse GetDeviceData with %s buffer: object %lu bytes, capacity %lu, flags 0x%lx",
                 rgdod ? "a" : "a NULL (count/flush)", (unsigned long)cbObjectData, (unsigned long)capacity,
                 (unsigned long)flags);
    }
    if (!s.initDone) return;
    if (s.failed) {  // still hand out the release queued by fail(), nothing else
        try {
            if (GetCurrentThreadId() == s.gameThread) deliverButtons(cbObjectData, rgdod, inOut, capacity, flags);
        } catch (...) {
        }
        return;
    }
    guarded("mouse data", [&] {
        if (!onGameThread()) return;
        tick();
        deliverButtons(cbObjectData, rgdod, inOut, capacity, flags);
    });
}

void onGetCursorPos(LPPOINT p) {
    State& s = S();
    ++s.nCursor;
    guarded("cursor", [&] {
        if (!p || !s.mouse.active || !s.hwnd || !onGameThread()) return;
        POINT pt{s.mouse.x, s.mouse.y};
        if (ClientToScreen(s.hwnd, &pt)) *p = pt;
    });
}

void onMessagePump() {
    State& s = S();
    ++s.nPump;
    guarded("message pump", [&] {
        if (onGameThread() && (!s.ticked || GetTickCount() - s.lastTickMs >= 50)) tick();
    });
}

void onQueueEmpty() {
    State& s = S();
    guarded("idle", [&] {
        if (!s.hwnd || !onGameThread()) return;
        const int ms = backgroundSleepMs(true, GetForegroundWindow() == s.hwnd, IsIconic(s.hwnd) != 0,
                                         s.cfg.backgroundSleepMs);
        if (ms > 0) Sleep(DWORD(ms));  // the client spins at full speed even behind other windows
    });
}

void onRecv(SOCKET sock, const char* buf, int n) {
    State& s = S();
    ++s.nRecv;
    guarded("recv", [&] {
        if (n <= 0) return;
        if (!onGameThread()) {
            noteUnseen(sock, false);
            return;
        }
        s.session->onServerData(uintptr_t(sock), reinterpret_cast<const uint8_t*>(buf), size_t(n), GetTickCount());
    });
}

int extendRecv(SOCKET sock, char* buf, int n, int cap) {
    State& s = S();
    int added = 0;
    guarded("recv extend", [&] {
        if (!buf || n <= 0 || cap <= n || !onGameThread()) return;
        added = int(s.session->appendForClient(uintptr_t(sock), reinterpret_cast<uint8_t*>(buf), size_t(n),
                                               size_t(cap), GetTickCount()));
        if (added == 0) return;
        if (!s.loggedMark) {
            s.loggedMark = true;
            LOG_INFO("target marks reach the client (%d bytes after %d received)", added, n);
        } else {
            LOG_DEBUG("target mark: %d bytes after %d received", added, n);
        }
    });
    return added;
}

void onSent(SOCKET sock, const char* buf, int n) {
    State& s = S();
    ++s.nSend;
    guarded("send", [&] {
        if (n <= 0) return;
        if (!onGameThread()) {
            noteUnseen(sock, true);
            return;
        }
        s.session->onClientSent(uintptr_t(sock), reinterpret_cast<const uint8_t*>(buf), size_t(n), GetTickCount());
        g_mapSocket.store(s.session->mapSocket());
        flushPackets();
    });
}

void onClose(SOCKET sock) {
    State& s = S();
    guarded("close", [&] {
        if (!onGameThread()) return;
        s.session->onClosed(uintptr_t(sock));
        g_mapSocket.store(s.session->mapSocket());
    });
}

DWORD mouseCooperativeLevel(DWORD flags) {
    State& s = S();
    if (!healthy() || !s.cfg.mouseFreedom || !(flags & DISCL_EXCLUSIVE)) return flags;
    const DWORD freed = (flags & ~DWORD(DISCL_EXCLUSIVE)) | DISCL_NONEXCLUSIVE;
    LOG_INFO("mouse freedom: cooperative level 0x%lx -> 0x%lx", (unsigned long)flags, (unsigned long)freed);
    return freed;
}

}  // namespace uapad::runtime
