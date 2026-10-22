#include "pad/win/client_npc.hpp"

#include <cstdint>
#include <string>

#include "pad/win/client_memory.hpp"

namespace uapad::clientnpc {
namespace {

using clientmem::load;

// UIWindowMgr 0x727ed8: its windows by kind - the NPC text (0x10), the menu (0x11), the number field (0x38).
constexpr uintptr_t kSayWnd = 0x727f7c;
constexpr uintptr_t kMenuWnd = 0x727f80;
constexpr uintptr_t kNumberWnd = 0x728038;
// Their classes: vtable, and its SendMsg (+0x94) - the code a click on their buttons ends in.
constexpr uintptr_t kSayVtable = 0x698cf0;
constexpr uintptr_t kSaySendMsg = 0x476ee0;
constexpr uintptr_t kMenuVtable = 0x699080;
constexpr uintptr_t kMenuSendMsg = 0x47b530;
constexpr uintptr_t kNumberVtable = 0x6989f8;
constexpr uintptr_t kNumberSendMsg = 0x4759e0;
constexpr uintptr_t kSendMsgSlot = 0x94;
// UIWindow: the button Enter presses (+0x58). The text window: the game message its button sends (+0x7c), the NPC
// (+0x80).
constexpr uintptr_t kDefaultButton = 0x58;
constexpr uintptr_t kSayMessage = 0x7c;
constexpr uintptr_t kSayNpc = 0x80;
// Button ids and window messages.
constexpr int kBtnNext = 0x98;
constexpr int kBtnClose = 0x87;
constexpr int kBtnOk = 0x76;
constexpr int kBtnCancel = 0x77;
constexpr int kMsgCommand = 6;  // a button was clicked: its id
constexpr int kMsgUp = 0x12;    // the arrow keys, passed on to the menu's list
constexpr int kMsgDown = 0x13;
// The game mode (CModeMgr 0x74b578: current mode +0x4, its kind +0x58, 1 = the game) and its SendMsg (+0x14);
// game message 0x66 closes the NPC's windows and tells the server, as the Close button does.
constexpr uintptr_t kModeNow = 0x74b57c;
constexpr uintptr_t kModeKind = 0x74b5d0;
constexpr uintptr_t kModeSendMsgSlot = 0x14;
constexpr int kGameMsgClose = 0x66;

using WndSendMsg = int(__attribute__((thiscall)) *)(void* self, void* sender, int msg, int a, int b, int c);
using ModeSendMsg = int(__attribute__((thiscall)) *)(void* self, int msg, int a, int b, int c);

// The window of `slot` when it is open and of the class we know.
uintptr_t window(uintptr_t slot, uintptr_t vtable) {
    uint32_t w = 0;
    uint32_t vt = 0;
    if (!load(slot, w) || w == 0 || !load(w, vt) || vt != vtable) return 0;
    return w;
}

bool send(uintptr_t w, int msg, int a) {
    if (w == 0) return false;
    uint32_t vt = 0;
    uint32_t fn = 0;
    if (!load(w, vt) || !load(vt + kSendMsgSlot, fn) || fn == 0) return false;
    reinterpret_cast<WndSendMsg>(uintptr_t(fn))(reinterpret_cast<void*>(w), nullptr, msg, a, 0, 0);
    return true;
}

// The text window's Close, also when it shows no button: the game's own way to end the talk.
bool closeTalk(uintptr_t say) {
    int32_t kind = 0;
    uint32_t mode = 0;
    uint32_t vt = 0;
    uint32_t fn = 0;
    int32_t npc = 0;
    if (!load(kModeKind, kind) || kind != 1 || !load(kModeNow, mode) || mode == 0 || !load(mode, vt) ||
        !load(vt + kModeSendMsgSlot, fn) || fn == 0 || !load(say + kSayNpc, npc))
        return false;
    reinterpret_cast<ModeSendMsg>(uintptr_t(fn))(reinterpret_cast<void*>(uintptr_t(mode)), kGameMsgClose, npc,
                                                 kBtnClose, 0);
    return true;
}

}  // namespace

bool supported() {
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    return load(kSayVtable + kSendMsgSlot, a) && a == kSaySendMsg && load(kMenuVtable + kSendMsgSlot, b) &&
           b == kMenuSendMsg && load(kNumberVtable + kSendMsgSlot, c) && c == kNumberSendMsg;
}

NpcDialog read() {
    if (window(kNumberWnd, kNumberVtable)) return NpcDialog::Number;
    if (window(kMenuWnd, kMenuVtable)) return NpcDialog::Menu;
    const uintptr_t say = window(kSayWnd, kSayVtable);
    if (!say) return NpcDialog::None;
    int32_t button = 0;
    load(say + kDefaultButton, button);
    if (button == kBtnNext) return NpcDialog::Next;
    if (button == kBtnClose) return NpcDialog::Close;
    return NpcDialog::Waiting;
}

bool answer(DialogCommand c) {
    const uintptr_t say = window(kSayWnd, kSayVtable);
    const uintptr_t menu = window(kMenuWnd, kMenuVtable);
    int32_t button = 0;
    int32_t message = 0;
    switch (c) {
        case DialogCommand::Next:
            return say && load(say + kDefaultButton, button) && button == kBtnNext && send(say, kMsgCommand, kBtnNext);
        case DialogCommand::Close:
            if (!say) return false;
            // The Close button sends what it was made for; a window without one (or one whose message is still
            // Next's) is closed the way that button would.
            if (load(say + kDefaultButton, button) && button == kBtnClose && load(say + kSayMessage, message) &&
                message == kGameMsgClose)
                return send(say, kMsgCommand, kBtnClose);
            return closeTalk(say);
        case DialogCommand::MenuUp: return send(menu, kMsgUp, 0);
        case DialogCommand::MenuDown: return send(menu, kMsgDown, 0);
        case DialogCommand::MenuOk: return send(menu, kMsgCommand, kBtnOk);
        case DialogCommand::MenuCancel: return send(menu, kMsgCommand, kBtnCancel);
        case DialogCommand::NumberOk: return send(window(kNumberWnd, kNumberVtable), kMsgCommand, kBtnOk);
    }
    return false;
}

void typeNumber(HWND game, int value) {
    if (!game) return;
    for (int i = 0; i < 12; ++i) PostMessageA(game, WM_CHAR, 8, 1);  // backspace: the field holds 9 digits at most
    for (char ch : std::to_string(value)) PostMessageA(game, WM_CHAR, WPARAM(uint8_t(ch)), 1);
}

}  // namespace uapad::clientnpc
