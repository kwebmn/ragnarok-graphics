#pragma once
#include <windows.h>

namespace uagfx::win {

// 3.0.3-beta.3 (the user, several game windows: the ones not played in light, their music off): which running game
// is the main one - a word the games share (Local\uaro-graphics-front) holds the process id of the one last in front.
class FrontGame {
public:
    ~FrontGame();
    // Every frame: this game is the main one - its window in front, or the game last in front is this one while
    // another program is; none known or that game gone, this one takes its place (one game of those left does).
    bool main(bool foreground);

private:
    bool tried_ = false;
    HANDLE map_ = nullptr;
    LONG* front_ = nullptr;
    LONG checked_ = 0;  // the other game seen running, and when
    ULONGLONG checkedAt_ = 0;
};

}  // namespace uagfx::win
