#include "win/real_ddraw.hpp"

#include "core/layout_migration.hpp"
#include "win/disk.hpp"
#include "win/pad_mount.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

#include "core/effect_groups.hpp"
#include "core/launcher.hpp"
#include "core/log_files.hpp"
#include "core/safe_mode.hpp"
#include "core/settings/ini_edit.hpp"
#include "win/start_screen.hpp"
#include "core/settings/report.hpp"
#include "core/update_check.hpp"
#include "core/version.hpp"
#include "win/crash.hpp"
#include "win/gpu/adapters.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/system_probe.hpp"
#include "win/window_mode.hpp"

namespace uagfx::win {
namespace {

HMODULE g_loaded = nullptr;  // set once the real DirectDraw is loaded

// 0.2.0 has the uaRO renderer; since 2.2 it is the only one (dgVoodoo is gone).
constexpr bool kUaroBuilt = true;

void logModule(const char* name) {
    if (HMODULE m = GetModuleHandleA(name)) LOG_INFO("%s: %s", name, modulePath(m).c_str());
}

// 0.51: [report] auto - the settings app waits for this game to end and, when its log says it went badly, sends a
// report to the developers (app/report_send.hpp). 2.10: at every start - it checks the site for an update first (the
// next start asks). The game itself never goes to the network.
void startWatcher(const std::string& dir) {
    const std::string exe = dir + "\\uaro-graphics.exe";
    if (!fileExists(exe)) {
        LOG_INFO("watcher: uaro-graphics.exe is not next to the game - no update check, no report");
        return;
    }
    std::string line = "\"" + exe + "\" --watch " + std::to_string(GetCurrentProcessId());
    STARTUPINFOA si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(exe.c_str(), &line[0], nullptr, nullptr, FALSE, BELOW_NORMAL_PRIORITY_CLASS, nullptr,
                        dir.c_str(), &si, &pi)) {
        LOG_ERROR("watcher: it would not start (error %lu)", GetLastError());
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    LOG_INFO("watcher: the settings app checks for an update and watches this game (reports, statistics)");
}

// The game's own arguments: its command line after the exe's path.
std::string gameArguments() {
    const char* line = GetCommandLineA();
    if (*line == '"') {
        line = std::strchr(line + 1, '"');
        line = line ? line + 1 : "";
    } else {
        while (*line && *line != ' ') ++line;
    }
    while (*line == ' ') ++line;
    return line;
}

// 2.11: UARO_LAUNCHER=1 - the launcher «Гра» (or the update's restart) started this game. Taken away: what the game
// starts (the watcher, the questions) does not inherit it.
bool takeLauncherMark() {
    char mark[8] = {};
    const DWORD n = GetEnvironmentVariableA(launcher::kStartedMark, mark, sizeof mark);
    SetEnvironmentVariableA(launcher::kStartedMark, nullptr);
    return n == 1 && mark[0] == '1';
}

// Seconds since the file was written, -1 when it is not there.
long secondsSince(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &a)) return -1;
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    const unsigned long long t =
        (unsigned long long)a.ftLastWriteTime.dwHighDateTime << 32 | a.ftLastWriteTime.dwLowDateTime;
    const unsigned long long n = (unsigned long long)now.dwHighDateTime << 32 | now.dwLowDateTime;
    return n > t ? long((n - t) / 10000000ull) : 0;
}

// 2.11: a direct start opens the launcher «Гра» instead (decision 13): its «Грати» starts the game again. false: it
// could not open, the game plays.
bool openLauncherInstead(const std::string& dir) {
    const std::string exe = dir + "\\uaro-graphics.exe";
    std::string line = "\"" + exe + "\"";
    STARTUPINFOA si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(exe.c_str(), &line[0], nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
        LOG_ERROR("launcher: «Гра» would not open (error %lu) - the game plays", GetLastError());
        return false;
    }
    AllowSetForegroundWindow(pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    HANDLE mark = CreateFileA(layout::join(dir, layout::kRedirected).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (mark != INVALID_HANDLE_VALUE) CloseHandle(mark);  // its time: when, for the next start (no bouncing)
    LOG_INFO("launcher: a direct start - «Гра» opens instead ([launcher] direct_start = 0); the game closes");
    TerminateProcess(GetCurrentProcess(), 0);
    return true;
}

void writeSmallFile(const std::string& path, const std::string& text) {
    HANDLE f = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD wrote = 0;
    WriteFile(f, text.data(), DWORD(text.size()), &wrote, nullptr);
    CloseHandle(f);
}

// 2.12: the safe mode (core/safe_mode) - the last start judged by its log, the failed ones in a row counted; two, or
// [renderer] safe_mode = 1, and this start's settings go safe. An automatic one stays (its note) until the settings
// app's «Звичайний режим» removes the note.
void safeModeFirst(const std::string& dir, Config& config) {
    std::string previous, kept;
    readTextFile(logfiles::previous(layout::join(dir, layout::now(layout::File::DriverLog))), previous);
    const std::string counter = layout::join(dir, layout::kFailedStarts);
    const std::string note = layout::join(dir, layout::kSafeModeNote);
    readTextFile(counter, kept);
    const int inRow = safe::failedInRow(std::atoi(kept.c_str()), safe::judge(previous));
    writeSmallFile(counter, std::to_string(inRow));
    const bool noted = fileExists(note);
    if (!noted && !safe::needed(inRow, config.safeMode)) return;
    if (!noted && !config.safeMode)
        writeSmallFile(note, std::to_string(inRow) + " starts in a row ended before their first picture");
    safe::apply(config);
    LOG_INFO("safe mode (%s): Direct3D 11, no effects, AI textures, DLSS, 64-bit renderer",
             config.safeMode ? "[renderer] safe_mode = 1" : "the last starts failed - the settings app tells");
}

// 2.10: the last check (the watcher's, at an earlier start) found an update - the question before the login window,
// asked by the settings app; its "Update" closes the game: the settings app updates it and starts it again.
void askUpdateFirst(const std::string& dir) {
    std::string info, version;
    if (!readTextFile(layout::join(dir, layout::kUpdateInfo), info) || !update::askAtStart(info, kVersion, version))
        return;
    const std::string exe = dir + "\\uaro-graphics.exe";
    std::string line =
        "\"" + exe + "\" --ask-update " + std::to_string(GetCurrentProcessId()) + " -- " + gameArguments();
    STARTUPINFOA si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!fileExists(exe) ||
        !CreateProcessA(exe.c_str(), &line[0], nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
        LOG_ERROR("update: %s is out, the question would not open (error %lu)", version.c_str(), GetLastError());
        return;
    }
    AllowSetForegroundWindow(pi.dwProcessId);
    LOG_INFO("update: %s is out - the player is asked", version.c_str());
    const DWORD waited = WaitForSingleObject(pi.hProcess, 10 * 60 * 1000);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (waited != WAIT_OBJECT_0 || code != update::kUpdateChosen) {
        LOG_INFO("update: played without it");
        return;
    }
    LOG_INFO("update: the game closes for it - the settings app updates it and starts it again");
    TerminateProcess(GetCurrentProcess(), 0);
}

HMODULE loadSystem() {
    const std::string path = systemDir() + "\\ddraw.dll";
    HMODULE m = LoadLibraryA(path.c_str());
    if (!m) LOG_ERROR("cannot load %s (error %lu)", path.c_str(), GetLastError());
    return m;
}

Startup decide() {
    Startup s;
    const std::string dir = gameDir();
    // 2.8: a 2.x install's files into uaro-graphics\ before anything reads them (core/layout_migration)
    WinDisk disk;
    const std::vector<std::string> moved = layout::migrate(dir, disk);
    std::string ini;
    const std::string iniPath = ourFile(dir, layout::File::Ini);
    const bool haveIni = readTextFile(iniPath, ini);
    s.config = parseConfig(ini);
    wlog::open(layout::join(dir, layout::now(layout::File::DriverLog)), s.config.logLevel);
    installCrashHandler(dir);  // 0.52: where a crash happened, and a minidump
    LOG_INFO("uaRO Graphics %s in %s", kVersion, modulePath(GetModuleHandleA(nullptr)).c_str());
    for (const std::string& line : moved) LOG_INFO("%s", line.c_str());
    LOG_INFO("%s", padMountedText());  // 2.9
    for (const char* folder : layout::kMadeAtStart) makeFolders(layout::join(dir, folder));
    safeModeFirst(dir, s.config);  // 2.12: before anything reads the renderer's settings
    {  // 2.12: the start panel in the player's language - the settings app's choice, else Windows'
        const auto keys = settings::readIni(ini);
        const auto chosen = keys.find("utility.language");
        startLanguage(settings::windowLang(chosen == keys.end() ? std::nullopt : settings::parseLang(chosen->second),
                                           PRIMARYLANGID(GetUserDefaultUILanguage())));
    }
    LOG_INFO("settings: %s; api = %s, mode = %s", haveIni ? iniPath.c_str() : "no uaro-graphics.ini, defaults",
             apiToken(s.config.api), displayModeToken(s.config.mode));
    // 2.11: the launcher's own start plays at once; a direct one opens «Гра», or with «Запускати гру одразу» asks
    // about an update first (2.10)
    if (!takeLauncherMark()) {
        const long since = secondsSince(layout::join(dir, layout::kRedirected));
        const bool redirect =
            launcher::redirect(false, s.config.directStart, fileExists(dir + "\\uaro-graphics.exe"), since);
        if (!redirect && since >= 0 && since < launcher::kRedirectGap)
            LOG_INFO("launcher: «Гра» opened %ld s ago and the game came back unmarked (an older uaro-graphics.exe?)"
                     " - it plays", since);
        if (!redirect || !openLauncherInstead(dir)) askUpdateFirst(dir);
    }
    LOG_INFO("display: high_resolution = %d, ui_filter = %s, render_scale = %d%%, sharpen = %d",
             s.config.highResolution ? 1 : 0, uiFilterToken(s.config.uiFilter), s.config.renderScale, s.config.sharpen);
    const WindowsVersion v = windowsVersion();
    LOG_INFO("Windows %lu.%lu build %lu", v.major, v.minor, v.build);
    s.computer = probeComputer();  // 2.5.15: the rest of the computer, for the reports and the statistics
    LOG_INFO("processor: %s; memory: %s; power: %s", processorText(s.computer).c_str(),
             memoryText(s.computer).c_str(), powerText(s.computer).c_str());
    if (!s.config.gpu.empty()) LOG_INFO("graphics card chosen: %s ([renderer] gpu)", s.config.gpu.c_str());
    logModule("dinput.dll");   // ROExt or uaRO Pad
    logModule("dinput8.dll");
    // The game's window black before anything that takes time - the Direct3D 12 probe, the renderer's start - so
    // that its white 640x480 start-up area does not stay on the screen meanwhile (window_mode.hpp).
    blackenGameWindow();

    gpu::setChosenAdapter(s.config.gpu);  // 0.50.2: the probes look at the card the renderer will take
    // 2.2.3: DXGI's 3072 MB in this 32-bit process is only a floor - the display driver's own number (the settings
    // app's since 0.51.1): every newer card came out weak, the automatic set the weak one while the app said strong
    const auto realMb = [](unsigned long long mb, const std::string& adapter) {
        if (mb != settings::kVramSeenAtMost) return mb;
        const unsigned long long real = settings::realVramMb(mb, registryVramMb(adapter));
        if (real != mb) LOG_INFO("video memory: %llu MB (the display driver's; DXGI tells a 32-bit program no more "
                                 "than %llu)", real, mb);
        return real;
    };
    SystemFacts facts;
    facts.windows10 = v.major >= 10;
    unsigned long long vramMb = 0;
    if (facts.windows10) {  // the log names the adapter either way
        const D3D12Probe p = probeD3D12();
        facts.d3d12 = p.ok;
        s.adapter = p.adapter;
        s.vendor = p.vendor;
        LOG_INFO("Direct3D 12: %s, adapter %s (vendor 0x%04x, device 0x%04x, %llu MB), hr 0x%08lx",
                 p.ok ? "OK" : "FAILED", p.adapter.c_str(), p.vendor, p.device, p.vramMb,
                 static_cast<unsigned long>(p.hr));
        vramMb = s.vramMb = realMb(p.vramMb, p.adapter);
    }
    // 0.46: Direct3D 11 - asked for, or (auto) where Direct3D 12 is not there.
    if (s.config.api == Api::D3d11 || (s.config.api == Api::Auto && !(facts.windows10 && facts.d3d12))) {
        const D3D11Probe p = probeD3D11();
        facts.d3d11 = p.ok;
        if (s.adapter.empty() || !facts.d3d12) {
            s.adapter = p.adapter;
            s.vendor = p.vendor;
            s.vramMb = realMb(p.vramMb, p.adapter);
        }
        LOG_INFO("Direct3D 11: %s, adapter %s, feature level %s, hr 0x%08lx", p.ok ? "OK" : "FAILED",
                 p.adapter.c_str(), p.ok ? featureLevelName(p.level).c_str() : "-", static_cast<unsigned long>(p.hr));
    }
    s.choice = chooseRenderer(s.config, facts, kUaroBuilt);
    s.cardLevel = effect_groups::cardLevel(s.choice.api == Api::D3d12 && facts.d3d12, vramMb);
    LOG_INFO("effects: set %s, the card's level %d", effect_groups::presetToken(s.config.effects.preset),
             s.cardLevel);
    // 2.2.1: also for the statistics - sent when the game ends, with the player's leave
    startWatcher(dir);  // 2.10: always - the update check; reports and statistics as the settings say
    LOG_INFO("renderer: %s%s (%s)", rendererName(s.choice.renderer),
             s.choice.renderer == Renderer::Uaro && s.choice.api == Api::D3d11 ? " with Direct3D 11" : "",
             s.choice.reason.c_str());
    return s;
}

HMODULE loadReal() {
    HMODULE m = loadSystem();  // 2.2: the system's only - dgVoodoo is no longer passed to
    if (m) LOG_INFO("passing DirectDraw to %s", modulePath(m).c_str());
    g_loaded = m;
    return m;
}

}  // namespace

const Startup& startup() {
    static const Startup s = decide();
    return s;
}

HMODULE realModule() {
    static HMODULE m = loadReal();
    return m;
}

FARPROC realProc(const char* name) {
    HMODULE m = realModule();
    return m ? GetProcAddress(m, name) : nullptr;
}

FARPROC loadedProc(const char* name) { return g_loaded ? GetProcAddress(g_loaded, name) : nullptr; }

void logModulesAtExit() {
    logModule("ddraw.dll");
}

}  // namespace uagfx::win
