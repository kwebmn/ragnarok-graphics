#include <windows.h>

#include <commctrl.h>
#include <objbase.h>

#include "app/main_window.hpp"

// uaro-graphics.exe: the uaRO Graphics Settings app (stage 2 spec).
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);  // ShellExecuteEx needs it
    uagfx::app::MainWindow window;
    const int code = window.run(inst, show);
    CoUninitialize();
    return code;
}
