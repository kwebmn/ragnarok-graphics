// uaro-studio.exe (2.3): uaRO Studio, the developers' and testers' tool for the texture pack's pictures and the
// driver's picture (docs/superpowers/specs/2026-10-18-texture-studio-design.md). 64-bit, linked statically.
#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>

#include <string>

#include "app/text.hpp"
#include "core/settings/ini_edit.hpp"
#include "studio/places.hpp"
#include "studio/window.hpp"
#include "win/paths.hpp"

using namespace uagfx;

namespace {

// The language the settings app was last left in ([utility] language of uaro-graphics.ini), else English.
settings::Lang languageOf(const std::wstring& gameDir) {
    std::string ini;
    if (!win::readTextFile(win::ourFile(app::ansi(gameDir), layout::File::Ini), ini)) return settings::Lang::En;
    const auto v = settings::readIni(ini);
    const auto it = v.find("utility.language");
    const auto lang = it == v.end() ? std::nullopt : settings::parseLang(it->second);
    return lang ? *lang : settings::Lang::En;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
    std::wstring gameDir;
    int argc = 0;
    if (LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc)) {
        for (int i = 1; i + 1 < argc; ++i)
            if (std::wstring(argv[i]) == L"--game") gameDir = argv[i + 1];
        LocalFree(argv);
    }
    if (gameDir.empty()) gameDir = studio::gameDirOf(studio::exePath());
    const settings::Lang lang = gameDir.empty() ? settings::Lang::En : languageOf(gameDir);
    const std::wstring title = app::wtr(lang, settings::Text::StudioTitle);
    if (gameDir.empty()) {
        MessageBoxW(nullptr, app::wtr(lang, settings::Text::StudioNoGame).c_str(), title.c_str(), MB_ICONINFORMATION);
        return 1;
    }
    if (!studio::runningFromCopy()) {  // G2: the shipped file stays free for the driver's update
        std::wstring error;
        if (studio::startCopy(gameDir, error)) return 0;
        MessageBoxW(nullptr, app::wtrf(lang, settings::Text::StudioCopyFailed, {app::utf8(error)}).c_str(),
                    title.c_str(), MB_ICONWARNING);
        return 1;
    }
    // One studio at a time: a second start brings the first to the front.
    HANDLE one = CreateMutexW(nullptr, FALSE, L"Local\\uaro-studio");
    if (one && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND first = FindWindowW(studio::kWindowClass, nullptr)) {
            if (IsIconic(first)) ShowWindow(first, SW_RESTORE);
            SetForegroundWindow(first);
        }
        CloseHandle(one);
        return 0;
    }
    studio::dropOldBuilds();
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_WIN95_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES |
                                             ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES};
    InitCommonControlsEx(&icc);
    studio::StudioWindow window(gameDir, lang);
    if (!window.create(instance, show)) return 1;
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (one) CloseHandle(one);
    return 0;
}
