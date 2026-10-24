#pragma once
#include <windows.h>

#include <vector>

#include "core/game_windows.hpp"

namespace uagfx::win {

// 3.0.3-beta.6: the block the running games share of their windows (core/game_windows) - this game's entry written,
// the live ones read back whole (an entry's seq odd while its game writes it).
class SharedWindows {
public:
    ~SharedWindows();
    void publish(const gamewindows::Entry& mine);  // into its number's place
    std::vector<gamewindows::Window> live();
    void leave(int number);  // this game's place free again

private:
    bool open();
    bool tried_ = false;
    HANDLE map_ = nullptr;
    gamewindows::Table* table_ = nullptr;
};

// That window in front, restored when minimised: Windows lets the program in front give the front away
bool switchTo(const gamewindows::Window& w);

}  // namespace uagfx::win
