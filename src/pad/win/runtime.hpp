#pragma once
#include <winsock2.h>
#include <windows.h>

#include <dinput.h>

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

}  // namespace uapad::runtime
