#pragma once
#include <windows.h>

#include <string>

#include "core/frame_pace.hpp"

// 3.0.22-beta.2 (the owner: «когда нажимаю на нпс, оно какбуто подлагивает»): the game's last frames (core/frame_pace
// Recent) by GetTickCount64 - an event's frames told in the log (an NPC's talk opening). The game's thread.
namespace uagfx::win::framerecent {

void add(const framepace::GameFrame& f);
// framepace::eventWords of the frames that ended within `halfMs` of `atMs`; "" none
std::string around(ULONGLONG atMs, ULONGLONG halfMs);

}  // namespace uagfx::win::framerecent
