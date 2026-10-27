#include <windows.h>

#include <commctrl.h>
#include <objbase.h>

#include "app/args.hpp"
#include "app/main_window.hpp"
#include "app/modes.hpp"
#include "app/report_send.hpp"
#include "app/text.hpp"

namespace {

// 2.13: the game's renderer writes the --report shot a frame or two after the press - waited for, 5 s at most
void waitForShot(const std::wstring& path) {
    unsigned long long last = ~0ull;
    for (int i = 0; i < 25 && !path.empty(); ++i) {
        WIN32_FILE_ATTRIBUTE_DATA a{};
        if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a)) {
            const unsigned long long size = (static_cast<unsigned long long>(a.nFileSizeHigh) << 32) | a.nFileSizeLow;
            if (size && size == last) return;  // written, and no longer growing
            last = size;
        }
        Sleep(200);
    }
}

}  // namespace

// uaro-graphics.exe: the uaRO Graphics Settings app (stage 2 spec). 0.51: "--watch <pid>" - no window: the driver's
// watcher of a game, for [report] auto (app/report_send.hpp). 2.10: the install, update, rollback, repair and removal
// modes (app/args, app/modes) - what the .bat files were.
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR line, int show) {
    uagfx::app::Args args = uagfx::app::parseArgs(line ? line : L"");
    if (args.mode == uagfx::app::Args::Mode::Settings && uagfx::app::carriesPackage())
        args.mode = uagfx::app::Args::Mode::Setup;  // uaro-graphics-setup.exe: the app with the package appended
    if (args.mode == uagfx::app::Args::Mode::Watch) {
        SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
        return args.pid ? uagfx::app::watchGame(args.pid, args.logSlot) : 1;
    }
    if (args.mode == uagfx::app::Args::Mode::TestReport)  // 3.0.5-beta.6: «Тест графіки»'s end, no window
        return uagfx::app::sendTestReport(args.testFile, args.logSlot);
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES |
                                          ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);  // ShellExecuteEx needs it
    int code = 0;
    if (args.mode != uagfx::app::Args::Mode::Settings) {
        code = uagfx::app::runMode(args);
    } else {
        uagfx::app::MainWindow window;
        if (args.report) {  // 2.13: the game's Ctrl+Shift+F10
            waitForShot(args.reportShot);
            uagfx::app::ReportStart start;
            start.fresh = true;
            start.shot = args.reportShot;
            start.map = uagfx::app::utf8(args.reportMap);
            start.logSlot = args.logSlot;  // 3.0.3-beta.1: the game window's logs
            window.reportAtStart(start);
        } else if (!args.answer.empty()) {  // 3.0.9-beta.4: the game's answer card - «Звернення» on that one
            uagfx::app::ReportStart start;
            start.code = uagfx::app::utf8(args.answer);
            window.reportAtStart(start);
        }
        code = window.run(inst, show);
    }
    CoUninitialize();
    return code;
}
