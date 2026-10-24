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
    // A place for a game whose log has no slot (its folder not writable): its own already, else the first free one
    // (no game's, or a game gone); 0 none
    int claim();
    // 3.0.3-beta.9: a game asked to get its picture whole (its process) - the ask's number; 0 no block
    int32_t summon(uint32_t pid);
    // The ask for this game, new since `seen` (set to it): true, its number in `serial`
    bool summonFor(uint32_t self, int32_t& seen, int32_t& serial);
    uint32_t readyOf(int number);  // the summon a window answered (its entry's, whole)

private:
    bool open();
    bool tried_ = false;
    HANDLE map_ = nullptr;
    gamewindows::Table* table_ = nullptr;
};

// That window in front, restored when minimised: Windows lets the program in front give the front away
bool switchTo(const gamewindows::Window& w);
// 3.0.3-beta.9: this game's wake (core/game_windows wakeName), made once - its frame limiter waits on it; and another
// game's woken
HANDLE ownWake();
void wake(uint32_t pid);

}  // namespace uagfx::win
