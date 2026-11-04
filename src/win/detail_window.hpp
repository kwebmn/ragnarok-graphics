#pragma once

namespace uagfx::win::detailwin {

// 3.0.11-beta.209 (the tester's third list: «alt d - якась менюшка з графоном - у старому стилі, але здається взагалі
// не потрібна»): [interface] detail_console - the game's «Arrange Detail» window (Alt+D, the old basic info's button,
// «Список команд»: UI window 0x39 at g_windowMgr + 0x168, class 0x698798 - object detail, shadows, trilinear, almost
// nothing with this renderer) closed as it opens, before the game draws it, and the driver's console opened instead.
// On the game's thread.
void frame(bool on);
void beforeGameDraw();

}  // namespace uagfx::win::detailwin
