#pragma once
#include <winsock2.h>
#include <windows.h>

#include <dinput.h>

#include <cstdint>
#include <vector>

#include "pad/core/config/bindings.hpp"
#include "pad/core/logic/npc_hint.hpp"

// Glue between the hooks and the portable core. Every entry point is fail-open:
// after an internal error it does nothing and the client keeps working.
namespace uapad::runtime {

void init();
bool healthy();
void setGameWindow(HWND hwnd);
// Mouse freedom: the cooperative level the mouse device should really get for the requested one.
DWORD mouseCooperativeLevel(DWORD flags);
void onMouseState(DWORD cb, LPVOID data);
void onMouseData(DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD inOut, DWORD capacity, DWORD flags);
void onGetCursorPos(LPPOINT p);
void onMessagePump();
// The client's PeekMessageA found no message: while the game is behind other windows, a short sleep.
void onQueueEmpty();
void onRecv(SOCKET sock, const char* buf, int n);
// Right after onRecv: appends client-only packets (the "!" over a new target) to the `n` received bytes
// in `buf` of `cap` bytes. Returns how many bytes it added; the client is told n + that many.
int extendRecv(SOCKET sock, char* buf, int n, int cap);
void onSent(SOCKET sock, const char* buf, int n);
void onClose(SOCKET sock);
// 3.0.11-beta.85: the one the pad's support skill would go on while it asks whom (the party panel fills its row);
// 0 none
uint32_t choosing();
// 3.0.11-beta.86 (the concept «Контроллер в интерфейсе uaRO», stage 2): what the interface shows of the controller
struct Look {
    bool shown = false;  // the player plays with it now (core/input_mode): its buttons instead of the keys' names
    bool sony = false;   // PlayStation's shapes and names
    Bindings bindings{};  // the layout in use
};
Look look();
// A key or a click in the game's window (window_switch): the keys' names back on the interface
void noteKeysOrMouse(bool click);
// 3.0.11-beta.87 (the concept's stage 3): the pad's target, and the monster a light tilt of the right stick points at
// (the rings under them, win/ground_marks); 0 none
uint32_t target();
uint32_t preview();
// 3.0.11-beta.88 (stage 4): [interface] pad_walk = arrow - the left stick walks without the mouse (win/context, each
// frame); where it walks now (the arrow's end, the map's cells); false none
void setDirectWalk(bool on);
bool walkTarget(int& x, int& y);
// 3.0.11-beta.90 (stage 6): [interface] pad_tiles - Start opens the driver's windows' tile (win/pad_tiles) instead of
// the pad's own menu; that menu opened from the tile's «Контролер»
void setTiles(bool on);
void openPadMenu();
// 3.0.11-beta.92: [interface] pad_hints on - the hint line (an NPC's window, a support skill asking whom) drawn by the
// interface itself (win/pad_hint) instead of the pad's own window; its parts now (none: no line)
void setOwnHints(bool on);
std::vector<HintPart> hints();

}  // namespace uapad::runtime
