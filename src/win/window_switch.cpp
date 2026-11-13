#include "win/key_nav.hpp"
#include "win/key_walk.hpp"
#include "win/walk_mode.hpp"
#include "win/window_switch.hpp"
#include "win/window_stack.hpp"

#include <windowsx.h>

#include <algorithm>

#include "core/double_click.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/world_mouse.hpp"
#include "win/answer_card.hpp"
#include "win/buff_column.hpp"
#include "win/char_block.hpp"
#include "win/uaui.hpp"
#include "win/hunt_panel.hpp"
#include "win/own_windows.hpp"
#include "win/recap_window.hpp"
#include "win/mvp_panel.hpp"
#include "win/merc_card.hpp"
#include "win/party_panel.hpp"
#include "win/hotbar.hpp"
#include "win/hud_edit.hpp"
#include "win/chat_window.hpp"
#include "win/item_window.hpp"
#include "win/login_card.hpp"
#include "win/pad_assign.hpp"
#include "win/pad_tiles.hpp"
#include "win/equip_window.hpp"
#include "win/npc_talk.hpp"
#include "win/shop_window.hpp"
#include "win/game_press.hpp"
#include "win/skill_window.hpp"
#include "win/stat_window.hpp"
#include "win/target_card.hpp"
#include "win/tracker.hpp"
#include "win/minimap_proto.hpp"
#include "win/reference_window.hpp"
#include "win/code_page.hpp"
#include "win/console.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/log_slot.hpp"
#include "win/notifier.hpp"

namespace uagfx::win {
namespace {

constexpr UINT_PTR kLeaveTimer = 0x75A9;  // the focus gone from the full screen: where to (WM_TIMER)
constexpr UINT_PTR kBehindTimer = 0x75AA;  // 3.0.3-beta.12: behind another window - the presents' work by the clock

WindowSwitch* g_switch = nullptr;
WNDPROC g_original = nullptr;
uint32_t g_diedAt = 0, g_hitAt = 0, g_whisperAt = 0;  // 3.0.4-beta.4: windowmarks' (the game's thread)

// 3.0.11-beta.33 (the owner: «при нажатии на кнопку закрыть справочник персонаж идет по локации»): a press one of the
// driver's parts took is not the game's until it is let go - the game reads the mouse (DirectInput) after this
// message, when the part may be gone already (its ✕ shuts it on the press), and saw the button go down over the world
void claimed(UINT m) {
    const bool swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;  // the messages' left is the device's right then
    switch (m) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: uapad::gamewindow::claimPress(swapped ? 2 : 1); break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK: uapad::gamewindow::claimPress(swapped ? 1 : 2); break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK: uapad::gamewindow::claimPress(4); break;
        default: break;
    }
}

// The driver's parts' mouse, the topmost first: true - the message was one of theirs
bool partsMouse(HWND w, UINT m, WPARAM wp, LPARAM lp, LRESULT& answer) {
    if (hotbarwin::held(w, m, wp, lp, answer) ||     // 3.0.11-beta.147: a hotbar cell's icon held - its let go
        hudedit::mouse(w, m, wp, lp, answer) ||      // 3.0.11-beta.75: all of it while the parts are framed
        padtileswin::mouse(w, m, wp, lp, answer) ||  // 3.0.11-beta.90: the controller's windows' tile (modal)
        padassignwin::mouse(w, m, wp, lp, answer) ||  // 3.0.11-beta.91: «Призначити» (modal)
        // 3.0.11-beta.195 (the tester: «панель вікон - виглядає наче зверху, а перетягнути не можна бо воно фактично
        // перекрите панеллю персонажа»): the windows' panel as it is drawn - over the panels, the windows, the card
        (g_switch && g_switch->mouse(w, m, lp, answer)) ||
        answercard::mouse(w, m, lp, answer) ||        // 3.0.9-beta.4: and over the answer card the card's
        loginwin::toastMouse(w, m, wp, lp, answer))   // 3.0.11-beta.175: «… додано на полицю · Скасувати»
        return true;
    // 3.0.11-beta.115: a press over a window of the game's own (drawn over the driver's since beta.109) is the game's
    if (gamepress::pressOver(m)) return false;
    if (minimapproto::mouseBig(w, m, wp, lp, answer)) return true;  // 3.0.11-beta.260: the big map over the windows
    if (windowstack::mouse(w, m, wp, lp, answer)) {  // 3.0.11-beta.108: the windows, the stack's top first
        if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN || m == WM_LBUTTONDBLCLK) keynav::click(true);  // .210
        if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN || m == WM_LBUTTONDBLCLK) minimapproto::lowerBig();  // .260
        return true;
    }
    return // the panels under the windows
           minimapproto::mouse(w, m, wp, lp, answer) ||  // 3.0.11-beta.1: the new minimap's
           buffcolumn::mouse(w, m, wp, lp, answer) ||    // 3.0.11-beta.35: the buffs' column's
           trackerwin::mouse(w, m, wp, lp, answer) ||    // 3.0.11-beta.70: the tracker's
           uaui::mouse(w, m, wp, lp, answer) ||          // 3.0.18-beta.7: the RmlUi spike's card
           charpanel::mouse(w, m, wp, lp, answer) ||     // 3.0.11-beta.36: the character block's
           targetpanel::mouse(w, m, wp, lp, answer) ||   // 3.0.11-beta.38: the target's card's
           partywin::mouse(w, m, wp, lp, answer) ||      // 3.0.11-beta.39: the party's panel's
           mvpwin::mouse(w, m, wp, lp, answer) ||  // 3.0.11-beta.280: the bosses' timers
           recapwin::mouse(w, m, wp, lp, answer) ||  // 3.0.18-beta.120
           huntwin::mouse(w, m, wp, lp, answer) ||  // 3.0.11-beta.278: «Полювання»
           merccardwin::blockMouse(w, m, wp, lp, answer) ||  // 3.0.11-beta.216: the mercenary's block's
           hotbarwin::mouse(w, m, wp, lp, answer) ||     // 3.0.11-beta.40: the hotbar's
           chatwin::mouse(w, m, wp, lp, answer);         // 3.0.11-beta.41: the chat's
}

// 3.0.16-beta.4 (a player: «жму 1 раз @@ ... все подвоює, а закрию кліен і зайду наново стає нормально»): a letter
// that comes twice for one key - the messages it came by, into the log (the facts before a cure): each key down
// counts the letters after it; a second one names its message, its code, the one before and how long between
struct InputWatch {
    int lettersSinceKey = 0;
    UINT lastMsg = 0;
    WPARAM lastWp = 0;
    ULONGLONG lastAt = 0;
    int told = 0;
    bool imeTold = false;
};
void watchInput(UINT m, WPARAM wp, LPARAM lp) {
    static InputWatch iw;
    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
        iw.lettersSinceKey = 0;
        return;
    }
    if (m == WM_IME_CHAR || m == WM_IME_COMPOSITION || m == WM_IME_STARTCOMPOSITION) {
        if (!iw.imeTold) {
            iw.imeTold = true;
            LOG_INFO("input: the keyboard's input method sends its own messages (0x%X) - letters may come by it too",
                     m);
        }
    }
    if (m != WM_CHAR && m != WM_IME_CHAR && m != WM_SYSCHAR) return;
    const ULONGLONG now = GetTickCount64();
    if (++iw.lettersSinceKey == 2 && iw.told < 20) {
        ++iw.told;
        LOG_INFO("input: a second letter for one key - 0x%X %u (lParam %08lX) after 0x%X %u %llu ms before", m,
                 unsigned(wp), static_cast<unsigned long>(lp), iw.lastMsg, unsigned(iw.lastWp),
                 static_cast<unsigned long long>(now - iw.lastAt));
    }
    iw.lastMsg = m;
    iw.lastWp = wp;
    iw.lastAt = now;
}

// 3.0.16-beta.5 (R-ZUR7S4: «подвоєння після вставки слова в вікно де треба ввести слово» - 1 typed, 11 in «Скільки
// штук»): the game's own WM_CHAR (0x6541E0, case 0x102) in the battle mode with its typing on (Enter, 0x772D58) puts
// a letter for a field other than the chat's in it (0x4F0520) and then once more at 0x654793 - every letter twice
// until Enter again. Its typing flag cleared for that one message: the letter once, the flag as it was after
struct BattleTyping {
    bool held = false;
};
BattleTyping holdBattleTyping(UINT m) {
    constexpr uint32_t kBattle = 0x772D54, kTyping = 0x772D58, kService = 0x74B560;
    constexpr uint32_t kWindowMgr = 0x727ED8, kFocus = kWindowMgr + 0x6C, kChat = kWindowMgr + 0x84;
    BattleTyping b;
    if (m != WM_CHAR) return b;
    uint32_t battle = 0, typing = 0, service = 0, focus = 0, chat = 0;
    if (!readGame(kBattle, &battle, 4) || !battle || !readGame(kTyping, &typing, 4) || !typing) return b;
    if (!readGame(kService, &service, 4) || service == 0xD) return b;  // there the game's own way: once
    readGame(kFocus, &focus, 4);
    readGame(kChat, &chat, 4);
    uint32_t chatWords = 0, chatOther = 0;
    if (chat) {
        readGame(chat + 0x68, &chatWords, 4);
        readGame(chat + 0x6C, &chatOther, 4);
    }
    if (!focus || (chat && (focus == chatWords || focus == chatOther))) return b;  // the chat's own field: once
    static int told = 0;
    if (told++ < 3) LOG_INFO("input: the battle mode's typing on - a letter for the field 0x%X put in once", focus);
    *reinterpret_cast<volatile uint32_t*>(uintptr_t(kTyping)) = 0;
    b.held = true;
    return b;
}

// 3.0.17-beta.20 (a player: «він вийшов с телепорта він активувався я хотів нажати індіфікатор а воно написали в чат
// У», GvG): the game's Enter (its windows' manager FUN_004d96e0, case 0xD) turns the battle mode's typing round on
// every Enter - one for an NPC's «Next», a Kafra's or a warp's menu, a message's OK too; the chat then took the next
// letters for the skills'. An Enter while another field has the keys leaves the typing as it was.
struct BattleEnter {
    bool held = false;
    uint32_t typing = 0;
};
BattleEnter holdBattleEnter(UINT m, WPARAM wp) {
    constexpr uint32_t kBattle = 0x772D54, kTyping = 0x772D58;
    constexpr uint32_t kWindowMgr = 0x727ED8, kFocus = kWindowMgr + 0x6C, kChat = kWindowMgr + 0x84;
    BattleEnter b;
    if (m != WM_KEYDOWN || wp != VK_RETURN) return b;
    uint32_t battle = 0, focus = 0, chat = 0, chatWords = 0, chatOther = 0;
    if (!readGame(kBattle, &battle, 4) || !battle || !readGame(kFocus, &focus, 4) || !focus) return b;
    if (readGame(kChat, &chat, 4) && chat) {
        readGame(chat + 0x68, &chatWords, 4);
        readGame(chat + 0x6C, &chatOther, 4);
    }
    if (focus == chatWords || focus == chatOther) return b;  // the chat's own Enter: its typing on or off
    if (!readGame(kTyping, &b.typing, 4)) return b;
    b.held = true;
    return b;
}

uint64_t g_lastEnter = 0;  // 3.0.18-beta.186: lastEnterMs()

LRESULT CALLBACK gameProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    watchInput(m, wp, lp);  // 3.0.16-beta.4
    // 3.0.18-beta.207: the window lost the keys - an arrow held stops walking
    if (m == WM_KILLFOCUS || (m == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE) || (m == WM_ACTIVATEAPP && !wp)) {
        keywalk::lost();
        walkmodewin::lost();  // 3.0.18-beta.208
        keynav::lost();       // 3.0.18-beta.210
    }
    if (m == WM_KEYDOWN && wp == VK_RETURN) g_lastEnter = GetTickCount64();
    if (g_switch) {
        if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN || m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN)
            uapad::runtime::noteKeysOrMouse(m != WM_KEYDOWN && m != WM_SYSKEYDOWN);  // 3.0.11-beta.86: keys' names back
        if (hudedit::key(m, wp, lp)) return 0;
        if (referencewin::fieldKey(m, wp, lp)) return 0;  // 3.0.18-beta.239: «Довідник»'s keys while it types
        if (uaui::key(m, wp, lp)) return 0;  // 3.0.18-beta.59: a field of uaUI's has the focus  // 3.0.11-beta.75: Ctrl+Shift+L - «Розміщення інтерфейсу», its Esc
        // 3.0.11-beta.108: the windows' keys, the top one's first (Ctrl+F «Довідник», «Речі»'s search, the shop's
        // and the NPC talk's keys); Esc closes the top one
        if (recapwin::key(m, wp, lp)) return 0;  // 3.0.18-beta.120: Esc closes «Що мене вбило»
        if (ownwin::key(m, wp, lp)) return 0;    // 3.0.18-beta.120: Ctrl+H/B/J/G/K - our windows
        if (hotbarwin::profileKeyPress(m, wp, lp)) return 0;  // 3.0.11-beta.288: Ctrl+Shift+1..3, the bar's profiles
        if (chatwin::searchKey(m, wp, lp)) return 0;  // 3.0.11-beta.284: Ctrl+F in the chat's field, its search
        npcwin::cutinEsc(m, wp);  // 3.0.13-beta.10: Esc closes the NPC's picture too (the key goes on)
        if (windowstack::key(m, wp, lp)) return 0;
        if (keynav::key(m, wp, lp)) return 0;  // 3.0.18-beta.210: the keys drive the top window once an arrow did
        if (walkmodewin::key(m, wp, lp)) return 0;  // 3.0.18-beta.208: the walk mode's keys and letters
        if (keywalk::key(m, wp, lp)) return 0;  // 3.0.18-beta.207: the arrows walk while the chat line is empty
        if (chatwin::key(m, wp, lp)) return 0;       // 3.0.11-beta.41: the new chat's F10, Alt+F10 and Tab
        if (charpanel::key(m, wp, lp)) return 0;     // 3.0.11-beta.141: Alt+V, the character block full or small
        hotbarwin::key(m, wp, lp);                   // 3.0.11-beta.45: its cells' keys shown pressed (still the game's)
        if (m == WM_KEYDOWN && minimapproto::key(unsigned(wp))) return 0;  // 3.0.11-beta.4: Ctrl+Tab, the new minimap's
        if ((m == WM_KEYDOWN || m == WM_SYSKEYDOWN) && g_switch->key(unsigned(wp), lp)) return 0;
        if (m == WM_ACTIVATE && g_switch->activate(w, wp)) return 1;  // the game's own answer to it
        if (m == WM_TIMER && g_switch->timer(w, wp)) return 0;
        if (notifier::message(w, m, wp, lp)) return 0;  // 3.0.4-beta.3: a click on a notification
        if (m == WM_DESTROY) notifier::gone(w);
        static int pressesTold = 0;  // 3.0.11-beta.43: whether the window's presses come at all, and where
        if ((m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN) && pressesTold < 6) {
            ++pressesTold;
            RECT client{};
            GetClientRect(w, &client);
            int cx = -1, cy = -1;
            readGameCursor(cx, cy);
            LOG_INFO("windows: a press (0x%X) at (%d, %d) of a %ldx%ld client, the game's cursor at (%d, %d)", m,
                     GET_X_LPARAM(lp), GET_Y_LPARAM(lp), client.right, client.bottom, cx, cy);
        }
        // 3.0.11-beta.202 (the tester: «треба повернути знімання дабл кліком по речі»): the game's window class sends
        // no double clicks (no CS_DBLCLKS) - the parts get one made of two left presses soon and near each other
        // (Windows' own time and box); the game its press as it came
        UINT pm = m;
        if (m == WM_LBUTTONDOWN) {
            static dblclick::Clicks clicks;
            if (dblclick::second(clicks, int64_t(GetTickCount64()), GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                                 int(GetDoubleClickTime()), GetSystemMetrics(SM_CXDOUBLECLK) / 2,
                                 GetSystemMetrics(SM_CYDOUBLECLK) / 2))
                pm = WM_LBUTTONDBLCLK;
        }
        LRESULT taken = 0;  // 3.0.11-beta.260: the open console's over everything
        if (Console::mouse(w, pm, wp, lp, taken)) {
            claimed(pm);
            return taken;
        }
        // 3.0.11-beta.77 (the owner: «чтобы без быстрых кнопок можно было двигать, чтобы сразу все мышкой»): a press
        // on a part held till it moves (the part dragged) or is let go (the part's click after all, given first)
        switch (hudedit::gate(w, pm, wp, lp)) {
            case hudedit::Gate::Take:
                claimed(pm);
                return 0;
            case hudedit::Gate::PressFirst: {
                UINT pm = 0;
                WPARAM pwp = 0;
                LPARAM plp = 0;
                hudedit::heldPress(pm, pwp, plp);
                LRESULT unused = 0;
                if (partsMouse(w, pm, pwp, plp, unused)) claimed(pm);
                break;
            }
            case hudedit::Gate::Pass: break;
        }
        LRESULT answer = 0;  // 3.0.3-beta.8: the mouse over the panel in the game's picture is the panel's
        if (partsMouse(w, pm, wp, lp, answer)) {
            claimed(pm);  // 3.0.11-beta.33: its press stays the part's until let go
            return answer;
        }
    }
    // 3.0.13-beta.11 (pick 42 of «50 идей»): a press the game gets - Shift on a player: followed for good; a plain one
    // on the world: the following ends (the game's own windows' presses aside); Esc too
    if (m == WM_LBUTTONDOWN && (wp & MK_SHIFT) && pointedActor()) uapad::runtime::followActor(pointedActor());
    else if (m == WM_LBUTTONDOWN && !(wp & MK_SHIFT) && cursorOverWorld()) {
        uapad::runtime::stopFollow("a click");
        keynav::click(false);  // 3.0.18-beta.210: the keys back to the world
    }
    else if (m == WM_KEYDOWN && wp == VK_ESCAPE) uapad::runtime::stopFollow("Esc");
    const BattleTyping typing = holdBattleTyping(m);
    const BattleEnter enter = holdBattleEnter(m, wp);
    const LRESULT r = CallWindowProcA(g_original, w, m, wp, lp);
    if (typing.held) *reinterpret_cast<volatile uint32_t*>(uintptr_t(0x772D58)) = 1;
    if (enter.held) {
        volatile uint32_t* now = reinterpret_cast<volatile uint32_t*>(uintptr_t(0x772D58));
        if (*now != enter.typing) {
            static int told = 0;
            if (told++ < 3)
                LOG_INFO("input: an Enter for another window - the battle mode's typing kept %s",
                         enter.typing ? "on" : "off");
            *now = enter.typing;
        }
    }
    return r;
}

// Window a is above window b (nearer the top of Windows' order)
bool above(HWND a, HWND b) {
    int steps = 0;
    for (HWND w = GetWindow(b, GW_HWNDPREV); w && steps < 1024; w = GetWindow(w, GW_HWNDPREV), ++steps)
        if (w == a) return true;
    return false;
}

}  // namespace

uint64_t lastEnterMs() { return g_lastEnter; }

namespace windowmarks {
void died() { g_diedAt = gamewindows::stamp(GetTickCount64()); }
void hit() { g_hitAt = gamewindows::stamp(GetTickCount64()); }
void whispered() { g_whisperAt = gamewindows::stamp(GetTickCount64()); }
}  // namespace windowmarks

WindowSwitch::WindowSwitch() {
    panel_.onPick = [this](int number) { to(number, "the panel"); };
}

WindowSwitch::~WindowSwitch() {
    if (g_switch == this) g_switch = nullptr;
    shared_.leave(int(mine_.number));
}

void WindowSwitch::check(HWND window, bool foreground, const std::string& name, const Config& config) {
    keys_ = config.windowKeys;
    if (!window_ && window) {  // the game's one window, ours to see its keys first
        const LONG_PTR prev = SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&gameProc));
        if (prev) {
            g_original = reinterpret_cast<WNDPROC>(prev);
            g_switch = this;
            window_ = window;
            LOG_INFO("windows: this game's window among the player's - its keys %s", windowKeysToken(keys_));
        }
    }
    mine_.pid = GetCurrentProcessId();
    mine_.window = uint32_t(reinterpret_cast<uintptr_t>(window));
    if (mine_.number == 0) mine_.number = uint32_t(wlog::slot() > 0 ? wlog::slot() : shared_.claim());
    if (mine_.number == 0) return;  // eight games run: no place for a ninth
    mine_.flags = fullScreen_ ? 0 : gamewindows::kDrawsBehind;  // on its full screen the game draws only in front
    gamewindows::putName(mine_, utf8Of(widened(name, gameCodePage(config))));
    charstatus::Status s;
    gamewindows::putStatus(mine_, !name.empty() && readOwnStatus(s) ? &s : nullptr);
    mine_.diedAt = g_diedAt;  // 3.0.4-beta.4
    mine_.hitAt = g_hitAt;
    mine_.whisperAt = g_whisperAt;
    publish(foreground);
    const std::vector<gamewindows::Window> all = shared_.live();
    // wholly behind another of the player's game windows, that window above it: nobody sees it
    bool hidden = false;
    RECT me{};
    if (window && !foreground && GetWindowRect(window, &me)) {
        const gamewindows::Box mine{me.left, me.top, me.right, me.bottom};
        for (const gamewindows::Window& w : all) {
            const HWND h = reinterpret_cast<HWND>(uintptr_t(w.window));
            RECT r{};
            if (w.pid == mine_.pid || !IsWindowVisible(h) || IsIconic(h) || !GetWindowRect(h, &r)) continue;
            if (gamewindows::covers(gamewindows::Box{r.left, r.top, r.right, r.bottom}, mine) && above(h, window)) {
                hidden = true;
                break;
            }
        }
    }
    hidden_ = hidden;
    panel_.update(all, mine_.pid);
}

void WindowSwitch::frame(HWND window, bool main, Config& config, double k, int targetWidth, int targetHeight,
                         bool fullScreen) {
    fullScreen_ = fullScreen;
    panel_.frame(window, main, config, k, targetWidth, targetHeight);
    if (!pendingNumber_) return;
    const uint64_t now = GetTickCount64();
    const uint32_t ready = shared_.readyOf(pendingNumber_);
    if (!gamewindows::switchNow(ready, uint32_t(pendingSerial_), pendingSince_, now)) return;
    const int number = pendingNumber_;
    pendingNumber_ = 0;
    show(number, pendingBy_, ready == uint32_t(pendingSerial_), now - pendingSince_);
}

void WindowSwitch::publish(bool frontNow) {
    if (frontNow) mine_.frontAt = GetTickCount64();
    if (mine_.number) shared_.publish(mine_);
}

bool WindowSwitch::key(unsigned vk, LPARAM lParam) {
    if (vk == VK_ESCAPE && panel_.closeMenu()) return true;  // 3.0.3-beta.10: its menu closed, not the game's opened
    const gamewindows::KeyAction a = gamewindows::keyAction(vk, GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_MENU) < 0,
                                                            GetKeyState(VK_SHIFT) < 0, keys_);
    if (a.action == gamewindows::Action::None || (a.action == gamewindows::Action::Panel && !panel_.on())) return false;
    if (lParam & (1 << 30)) return true;  // held down: taken as its first press was, nothing done again
    if (a.action == gamewindows::Action::Panel)
        panel_.toggle();  // 3.0.3-beta.7: Ctrl+Alt+0 shows and hides the windows' panel
    else
        act(a);
    return true;
}

void WindowSwitch::act(const gamewindows::KeyAction& a) {
    if (a.action == gamewindows::Action::Window) {
        to(a.number, "Ctrl+Alt+digit");
        return;
    }
    const std::vector<gamewindows::Window> all = shared_.live();
    const gamewindows::Window* back = gamewindows::previous(all, GetCurrentProcessId());
    if (back)
        to(back->number, "Ctrl+Tab");
    else
        LOG_INFO("windows: Ctrl+Tab - no other game window");
}

void WindowSwitch::to(int number, const char* by) {
    const DWORD self = GetCurrentProcessId();
    const std::vector<gamewindows::Window> all = shared_.live();
    const gamewindows::Window* w = gamewindows::byNumber(all, number);
    if (!w || w->pid == self) {
        LOG_INFO("windows: %s, window %d - %s", by, number, w ? "this window" : "no game window of that number");
        return;
    }
    publish(true);  // in front till now: Ctrl+Tab in the other one comes back here
    // 3.0.3-beta.9: one that draws behind this one is asked to get its picture whole first, and woken to it; a
    // minimised one, or one on its full screen (it draws nothing while not active), is shown at once
    const bool asks = w->drawsBehind && !IsIconic(reinterpret_cast<HWND>(uintptr_t(w->window)));
    const int32_t serial = asks ? shared_.summon(w->pid) : 0;
    if (!serial) {
        show(number, by, false, 0);
        return;
    }
    pendingNumber_ = number;
    pendingSerial_ = serial;
    pendingSince_ = GetTickCount64();
    pendingBy_ = by;
    wake(w->pid);
}

void WindowSwitch::show(int number, const char* by, bool ready, uint64_t waited) {
    const std::vector<gamewindows::Window> all = shared_.live();
    const gamewindows::Window* w = gamewindows::byNumber(all, number);
    if (!w) {
        LOG_INFO("windows: %s - window %d is gone", by, number);
        return;
    }
    const bool ok = switchTo(*w);
    LOG_INFO("windows: %s - to window %d, %s in %llu ms%s", by, number,
             ready ? "its picture whole" : "shown as it is", static_cast<unsigned long long>(waited),
             ok ? "" : " (Windows did not bring it to the front)");
}

bool WindowSwitch::summoned(bool foreground) {
    int32_t serial = 0;
    if (shared_.summonFor(GetCurrentProcessId(), seenSummon_, serial)) {
        summonSerial_ = serial;
        summonedAt_ = GetTickCount64();
        wholeFrames_ = 0;
    }
    if (!summonedAt_) return false;
    if (foreground || GetTickCount64() - summonedAt_ >= gamewindows::kSummonKeepMs) {
        summonedAt_ = 0;  // in front now (the main one by itself), or the ask is old
        return false;
    }
    return true;
}

void WindowSwitch::presented(bool light) {
    if (!summonedAt_ || light || ++wholeFrames_ != 3) return;  // a frame or two for a new size and DLSS's history
    mine_.ready = uint32_t(summonSerial_);
    publish(false);
}

bool WindowSwitch::activate(HWND window, WPARAM wp) {
    if (LOWORD(wp) == WA_INACTIVE) {  // 3.0.3-beta.11: the panel's menu closed, the frame counter hidden
        panel_.closeMenu();
        if (onLeave) onLeave();
        SetTimer(window, kBehindTimer, 100, nullptr);  // 3.0.3-beta.12
    } else {
        KillTimer(window, kBehindTimer);
    }
    if (!fullScreen_) return false;  // in a window the game does nothing there
    if (LOWORD(wp) == WA_INACTIVE) {  // not minimised at once: where the focus went decides (timer)
        leaveSince_ = GetTickCount64();
        SetTimer(window, kLeaveTimer, 50, nullptr);
        return true;
    }
    KillTimer(window, kLeaveTimer);
    leaveSince_ = 0;
    return !IsIconic(window);  // shown already: nothing to maximise; minimised (back from the desktop): the game's own
}

bool WindowSwitch::timer(HWND window, WPARAM id) {
    if (id == kBehindTimer) {  // 3.0.3-beta.12: its sounds' fade and its entry, while it presents no more
        if (GetForegroundWindow() == window)
            KillTimer(window, kBehindTimer);  // in front again: its presents do it
        else if (onBehind)
            onBehind();
        return true;
    }
    if (id != kLeaveTimer) return false;
    const HWND front = GetForegroundWindow();
    DWORD pid = 0;
    char cls[64] = {};
    if (front) {
        GetWindowThreadProcessId(front, &pid);
        GetClassNameA(front, cls, sizeof cls);
    }
    const gamewindows::Leave l = gamewindows::leaving(pid, cls, shared_.live(), GetCurrentProcessId());
    if (l == gamewindows::Leave::Wait && GetTickCount64() - leaveSince_ < 3000) return true;  // asked again
    KillTimer(window, kLeaveTimer);
    leaveSince_ = 0;
    if (l == gamewindows::Leave::Stay) return true;  // another game window: no blink
    ShowWindow(window, SW_MINIMIZE);  // the desktop, another program: as the game itself does
    return true;
}

}  // namespace uagfx::win
