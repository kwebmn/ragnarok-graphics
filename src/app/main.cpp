#include <windows.h>

#include <commctrl.h>
#include <objbase.h>

#include <cwchar>

#include "app/main_window.hpp"
#include "app/report_send.hpp"

// uaro-graphics.exe: the uaRO Graphics Settings app (stage 2 spec). 0.51: "--watch <pid>" - no window: the driver's
// watcher of a game, for [report] auto (app/report_send.hpp).
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR line, int show) {
    if (line) {
        const wchar_t* w = std::wcsstr(line, L"--watch ");
        if (w) {
            const unsigned long pid = std::wcstoul(w + 8, nullptr, 10);
            SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
            return pid ? uagfx::app::watchGame(pid) : 1;
        }
    }
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES |
                                          ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);  // ShellExecuteEx needs it
    uagfx::app::MainWindow window;
    const int code = window.run(inst, show);
    CoUninitialize();
    return code;
}
