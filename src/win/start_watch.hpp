#pragma once

namespace uagfx::win::startwatch {

// 3.0.18-beta.223 (R-H42AS4, R-3PETVR «процес є, а гра так і не запускається»: the game stopped before its DirectDraw,
// where the driver's log opens - the reports had no log at all): a watch from the start (DllMain, on the game's main
// thread). 25 s without the game's DirectDraw and the log opens: where the main thread stands (sampled a few times,
// its calls by module), the command line, the modules in the process (another program's?), the main thread's windows
// (a message box hidden or off every screen - brought to the front), a minidump for the report. The DirectDraw made
// after all is told too.
void install();  // DllMain, DLL_PROCESS_ATTACH
void passed();   // the game's DirectDrawCreateEx

}  // namespace uagfx::win::startwatch
