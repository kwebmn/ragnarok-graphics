#pragma once

// The uaRO client keeps a cursor of its own and only adds DirectInput deltas to it: where that cursor is and
// the game's resolution, read from its memory, put the cursor exactly where the pad wants it.
namespace uapad::clientmouse {

// True when the running exe is the uaRO build whose layout we know (checked by code signatures).
bool supported();
// The client's cursor and the game's resolution in its pixels. False when unreadable or out of range.
bool read(int& x, int& y, int& width, int& height);

}  // namespace uapad::clientmouse
