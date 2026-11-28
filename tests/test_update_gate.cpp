#include <string>

#include "core/update_gate.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.24-beta.45 (the owner: «когда нажал руками обновить оно чего-то не закрыло лаунчер и начало устанавливать, а
// установка при открытом лаунчере не работает»): the update waited at most a minute for the app that started it and
// then put its files in with the app open. Now its files go in with the game and the settings app closed - the game
// waited for (the player closes it), the app asked to close (once a window) and then waited for.
TEST(update_gate_installs_only_with_the_game_and_the_app_closed) {
    using update_gate::Next;
    CHECK(update_gate::next(false, false, false) == Next::Install);
    CHECK(update_gate::next(true, false, false) == Next::WaitGame);
    CHECK(update_gate::next(true, true, false) == Next::WaitGame);  // the game first, the app asked after it
    CHECK(update_gate::next(false, true, false) == Next::CloseApp);
    CHECK(update_gate::next(false, true, true) == Next::WaitApp);  // asked once: its question (if any) is the player's
    CHECK(update_gate::next(false, false, true) == Next::Install);
}

// The update's own log (uaro-graphics\logs\uaro-graphics-update.log, readable over the watchdog's files): a line a
// step; past its cap the oldest whole lines go
TEST(update_log_line_and_cap) {
    CHECK_EQ(update_gate::logLine("2026-11-28 05:01:46", "started\r\nby the app"),
             std::string("2026-11-28 05:01:46 started  by the app\r\n"));
    const std::string log = "a 1\r\nb 2\r\nc 3\r\n";
    CHECK_EQ(update_gate::capped(log, 100), log);  // under the cap: as it is
    CHECK_EQ(update_gate::capped(log, 12), std::string("c 3\r\n"));  // the newest half, from a line's start
    CHECK_EQ(update_gate::capped("abcdefghij", 4), std::string());  // one long line: nothing whole kept
}
