#pragma once
#include <string>

namespace uagfx::win {

// 0.52: when the game crashes - an exception nobody handles - the log says where (the module and the offset in it)
// and uaro-graphics\crash.dmp keeps a small minidump (dbghelp.dll, loaded only then); the report to the developers
// takes it along (app/report_send.cpp). The filter set before ours, if any, runs after it.
// 3.0.20-beta.83: uaro-render64.exe's own dump is uaro-graphics\crash-render64.dmp (`dumpName`)
void installCrashHandler(const std::string& gameDir, const char* dumpName = "crash.dmp");
// 3.0.18-beta.2: the game hung (win/profiler: 15 s without a beat) - a minidump of every thread into the same
// uaro-graphics\crash.dmp (the report sends it when the game ends badly); false when it could not be written
bool writeHangDump();
// 3.0.22-beta.8: dbghelp.dll loaded and its MiniDumpWriteDump found now, at a quiet moment - a dump written later from
// a watch while another thread stands somewhere (the 64-bit renderer's, host/hang_watch) asks the loader nothing then;
// false: no dbghelp.dll
bool prepareHangDump();

}  // namespace uagfx::win
