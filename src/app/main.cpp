#include <windows.h>

#include <commctrl.h>
#include <objbase.h>

#include "app/args.hpp"
#include "app/main_window.hpp"
#include "app/modes.hpp"
#include "app/report_send.hpp"

// uaro-graphics.exe: the uaRO Graphics Settings app (stage 2 spec). 0.51: "--watch <pid>" - no window: the driver's
// watcher of a game, for [report] auto (app/report_send.hpp). 2.10: the install, update, rollback, repair and removal
// modes (app/args, app/modes) - what the .bat files were.
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR line, int show) {
    uagfx::app::Args args = uagfx::app::parseArgs(line ? line : L"");
    if (args.mode == uagfx::app::Args::Mode::Settings && uagfx::app::carriesPackage())
        args.mode = uagfx::app::Args::Mode::Setup;  // uaro-graphics-setup.exe: the app with the package appended
    if (args.mode == uagfx::app::Args::Mode::Watch) {
        SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
        return args.pid ? uagfx::app::watchGame(args.pid) : 1;
    }
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES |
                                          ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);  // ShellExecuteEx needs it
    int code = 0;
    if (args.mode != uagfx::app::Args::Mode::Settings) {
        code = uagfx::app::runMode(args);
    } else {
        uagfx::app::MainWindow window;
        code = window.run(inst, show);
    }
    CoUninitialize();
    return code;
}
