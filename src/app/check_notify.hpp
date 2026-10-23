#pragma once
#include <windows.h>

namespace uagfx::app {

// 3.0.1-beta.8: the start's checks (the update, «Гра»'s news, the client's patch, the reports) wake the window the
// moment their answer is in - the strip and the pages waited for its 2-second timer before (the user: «достаточно
// долго проверяется обновление»).
void setCheckWindow(HWND window, UINT message);
void checkDone();  // a check's thread, its answer set: the window's message posted

}  // namespace uagfx::app
