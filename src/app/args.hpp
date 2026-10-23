#pragma once
#include <string>

namespace uagfx::app {

// 2.10: uaro-graphics.exe's modes on its command line - what used to be .bat files runs as a process of its own with a
// window of its own: install (an older update.bat's install.bat), update, the question at the game's start,
// rollback, repair, removal (and its second half from a copy in %TEMP%).
struct Args {
    enum class Mode { Settings, Watch, Install, Update, AskUpdate, Rollback, Repair, Uninstall, UninstallRun, Setup };
    Mode mode = Mode::Settings;
    unsigned long pid = 0;  // --watch N, --ask-update N, --after N: the process waited for
    bool quiet = false;     // --install --quiet (or /quiet): no message at the end
    enum class Then { Nothing, App, Game };
    Then then = Then::Nothing;  // --then-app, --then-game: started at the end
    bool keep = true;           // --uninstall-run --keep 0|1: the settings and the pack stay
    std::wstring dir;           // --uninstall-run, --setup: --dir "<the game's folder>"
    std::wstring gameArgs;      // everything after " -- ", verbatim: uaRO.exe's arguments
    // 2.13: --report ["<shot>"] [--map <map>] - the game's Ctrl+Shift+F10: the settings app with «Нове звернення»
    bool report = false;
    std::wstring reportShot, reportMap;
    // 3.0.3-beta.1: --log-slot N - the game window whose logs these are (core/log_files slotted; 1-8, else 1)
    int logSlot = 1;
};

Args parseArgs(const std::wstring& line);
std::wstring argsLine(const Args& a);  // parseArgs(argsLine(a)) gives a back

}  // namespace uagfx::app
