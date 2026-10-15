#pragma once
#include <string>

namespace uagfx::win {

// 0.52: when the game crashes - an exception nobody handles - the log says where (the module and the offset in it)
// and uaro-graphics\crash.dmp keeps a small minidump (dbghelp.dll, loaded only then); the report to the developers
// takes it along (app/report_send.cpp). The filter set before ours, if any, runs after it.
void installCrashHandler(const std::string& gameDir);

}  // namespace uagfx::win
