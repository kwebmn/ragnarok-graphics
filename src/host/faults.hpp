#pragma once

// 3.0.20-beta.83 (R-EGT98G: the 64-bit renderer gone with exit code 0xC0000409 five seconds after a map's first
// frame, twice - on Windows 10 msvcrt.dll ends a process so, with no handler of its own run, when one of its functions
// is handed a wrong value): msvcrt's invalid parameter handler of the renderer's - the call refused instead (msvcrt
// returns its error to the caller), the calls on the thread's stack told and a dump written the first time
namespace uagfx::host::faults {

void install();  // once, after the log is open and the crash handler set (win/crash.hpp: crash-render64.dmp)

}  // namespace uagfx::host::faults
