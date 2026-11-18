#pragma once
#include <cstdint>
#include <string>

// 3.0.20-beta.76 (the owner's log of 16.11 22:40:54-22:40:59, beta.68: the 64-bit renderer silent for five seconds
// at a map's first frames - the moment DLSS's start in its background ends -, then gone with exit code 0xC0000409;
// a fast fail runs no handler of ours and leaves no dump, and the log told nothing of those five seconds): the
// renderer's own thread watched. A message it stays in for over two seconds is told in the log with where the thread
// stands and the calls on its stack (core/stack_scan: read from the stack's words of the thread held still for a
// moment - no unwinding, no locks), again at six and at twenty seconds, and its end when it moves on. The lines go
// to uaro-render64.log at once and to the game's log when the bridge has room.
namespace uagfx::host::hangwatch {

void start();              // once, on the renderer's own thread (the one that takes the messages)
// A message taken: its type (0: the look at the background start); 3.0.22-beta.8: a texture's creation with its size
// (told with it; core/hang_policy: from 1 s, with the renderer's own minidump)
void busy(uint32_t what, int width = 0, int height = 0, int levels = 0);
void idle();               // done with it
void stop();
// 3.0.20-beta.83 (host/faults.cpp): the calls on the calling thread's own stack, the newest first, named by the
// images loaded now ("a.dll+0x1 < b.exe+0x2")
std::string callsHere();

}  // namespace uagfx::host::hangwatch
