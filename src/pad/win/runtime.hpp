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

// 3.0.12-beta.1: false - [pad] enabled = 0: the runtime runs for the mouse only, no controller read (before init)
void allowController(bool allowed);
// 3.0.13-beta.11 (pick 42 of «50 идей»): [interface] follow - the player Shift+clicked followed for good (the
// controller's follow, core/logic); stopped by a plain click on the world, Esc, or the leader out of sight
void setFollow(bool on);
void followActor(uint32_t id);
void stopFollow(const char* why);
bool following();
void init();
bool healthy();
void setGameWindow(HWND hwnd);
// Mouse freedom: the cooperative level the mouse device should really get for the requested one.
DWORD mouseCooperativeLevel(DWORD flags);
void onMouseState(DWORD cb, LPVOID data);
// 3.0.19-beta.11 (R-ZQWGVD, the 3D view - win/view3d): while on, the game's cursor is held at (x, y) of the game's
// screen at every poll of the mouse - its own moves (DirectInput's counts) kept for takeLook instead of moving the
// cursor; off: the cursor follows the Windows one again (mouse freedom). On the game's thread.
void holdCursor(bool on, int x, int y);
// The mouse's moves since the last take; false: no poll held yet (the runtime not on the mouse - the cursor's own move
// is the view's then)
bool takeLook(long& dx, long& dy);
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
// 3.0.18-beta.210 (the keyboard concept, stage 4): look() with `shown` while the keys drive a window too (win/key_nav)
// - the windows' gold ring and hints (their buttons' names the keys' then, padglyph key caps)
Look navLook();
// A key or a click in the game's window (window_switch): the keys' names back on the interface
void noteKeysOrMouse(bool click);
// 3.0.11-beta.87 (the concept's stage 3): the pad's target, and the monster a light tilt of the right stick points at
// (the rings under them, win/ground_marks); 0 none
uint32_t target();
uint32_t preview();
// 3.0.11-beta.256 (the NPC concept, stage 2): the NPC the Talk button takes now (0 none, or in a fight), the other
// NPCs within its reach, and the Talk button (a kPad bit, 0 none bound)
uint32_t npcTag();
std::vector<uint32_t> npcNear();
uint16_t talkButtonBit();
// 3.0.11-beta.88 (stage 4): [interface] pad_walk = arrow - the left stick walks without the mouse (win/context, each
// frame); where it walks now (the arrow's end, the map's cells); false none
void setDirectWalk(bool on);
// 3.0.18-beta.209 (the keyboard concept, stage 3): the keys' quick actions for the next tick (win/key_walk,
// win/walk_mode) - fired by the pad's controller as its buttons' (with the pad off too); Tab's target; the camera
// turned (degrees, + right as the right stick) and zoomed (notches, + closer)
void keyAction(const Action& a);
void keyCycleTarget(int dir);
void keyCamera(float turnDeg, int zoomSteps);
bool walkTarget(int& x, int& y);
// 3.0.11-beta.145 (the concept «маг, прист и хант на геймпаде»): a ground skill aimed with the pad now - the skill, its
// level and the cell it would go on (its cells on the ground: win/ground_marks); false while not aiming
bool aimCells(uint16_t& skill, int& lv, int& x, int& y);
// 3.0.18-beta.132 (the pad analysis, variant A): a skill wheel held now - which (0, 1) and the sector pointed at (-1
// none); false while none
bool wheelNow(int& index, int& sector);
// 3.0.11-beta.103: the stick's way on the ground while it walks (east, north: a unit) and whether a wall cuts the cell
// asked for short (the arrow's red bar); false none. 3.0.11-beta.104: the march's way (a slide's along a wall) and the
// lead (cells)
bool walkWay(float& east, float& north, bool& cut, int& lead);
// 3.0.11-beta.90 (stage 6): [interface] pad_tiles - Start opens the driver's windows' tile (win/pad_tiles) instead of
// the pad's own menu; that menu opened from the tile's «Контролер»
void setTiles(bool on);
void openPadMenu();
// 3.0.11-beta.92: [interface] pad_hints on - the hint line (an NPC's window, a support skill asking whom) drawn by the
// interface itself (win/pad_hint) instead of the pad's own window; its parts now (none: no line)
void setOwnHints(bool on);
// 3.0.11-beta.95 (the owner on the ring: «устраивает»): the interface draws the target's ring - the pad's fake mark off
void setRing(bool drawn);
std::vector<HintPart> hints();

}  // namespace uapad::runtime
