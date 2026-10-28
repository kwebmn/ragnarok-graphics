#include "win/real_ddraw.hpp"

#include "core/layout_migration.hpp"
#include "win/disk.hpp"
#include "win/pad_mount.hpp"
#include "win/roext.hpp"
#include "win/updates_hook.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

#include "core/card_speed.hpp"
#include "core/effect_groups.hpp"
#include "core/graphics_test.hpp"
#include "core/launcher.hpp"
#include "core/log_files.hpp"
#include "core/safe_mode.hpp"
#include "core/settings/auto_setup.hpp"
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
    if (wlog::slot() > 1) line += " --log-slot " + std::to_string(wlog::slot());  // 3.0.3-beta.1: this window's logs
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

}  // namespace

void startReport(const std::string& shot, const std::string& map) {
    const std::string dir = gameDir();
    const std::string exe = dir + "\\uaro-graphics.exe";
    if (!fileExists(exe)) {
        LOG_INFO("report: uaro-graphics.exe is not next to the game - no «Звернення»");
        return;
    }
    std::string line = "\"" + exe + "\" --report \"" + shot + "\"";
    if (!map.empty()) line += " --map " + map;
    if (wlog::slot() > 1) line += " --log-slot " + std::to_string(wlog::slot());  // 3.0.3-beta.1: this window's logs
    STARTUPINFOA si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    // started by the game in front: Windows lets the app's window come in front of it
    if (!CreateProcessA(exe.c_str(), &line[0], nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
        LOG_ERROR("report: uaro-graphics.exe would not start (error %lu)", GetLastError());
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    LOG_INFO("report: «Нове звернення» with %s%s%s", shot.c_str(), map.empty() ? "" : " on ", map.c_str());
}

// 3.0.9-beta.4: the game's answer card - «Звернення» on that conversation (the code is the site's: answers::read)
void startAnswer(const std::string& code) {
    const std::string dir = gameDir();
    const std::string exe = dir + "\\uaro-graphics.exe";
    if (!fileExists(exe)) {
        LOG_INFO("answers: uaro-graphics.exe is not next to the game - %s not opened", code.c_str());
        return;
    }
    std::string line = "\"" + exe + "\" --answer " + code;
    STARTUPINFOA si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(exe.c_str(), &line[0], nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
        LOG_ERROR("answers: uaro-graphics.exe would not start (error %lu)", GetLastError());
        return;
    }
    AllowSetForegroundWindow(pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

bool startTestReport(const std::string& file) {
    const std::string exe = gameDir() + "\\uaro-graphics.exe";
    if (!fileExists(exe)) {
        LOG_INFO("test: uaro-graphics.exe is not next to the game - the result stays in %s", file.c_str());
        return false;
    }
    std::string line = "\"" + exe + "\" --test-report \"" + file + "\"";
    if (wlog::slot() > 1) line += " --log-slot " + std::to_string(wlog::slot());
    STARTUPINFOA si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(exe.c_str(), &line[0], nullptr, nullptr, FALSE, BELOW_NORMAL_PRIORITY_CLASS, nullptr,
                        gameDir().c_str(), &si, &pi)) {
        LOG_ERROR("test: uaro-graphics.exe would not start (error %lu)", GetLastError());
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    LOG_INFO("test: the settings app sends the result (%s)", file.c_str());
    return true;
}

namespace {

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
bool takeMark(const char* name) {
    char mark[8] = {};
    const DWORD n = GetEnvironmentVariableA(name, mark, sizeof mark);
    SetEnvironmentVariableA(name, nullptr);
    return n == 1 && mark[0] == '1';
}

bool takeLauncherMark() { return takeMark(launcher::kStartedMark); }

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
    // 3.0.3-beta.1: the last game of this log's slot - a second window's start judges the second window's last one
    const std::string log =
        wlog::path().empty() ? layout::join(dir, layout::now(layout::File::DriverLog)) : wlog::path();
    readTextFile(logfiles::previous(log), previous);
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
    const bool back = update::backToRelease(update::keptChannel(info, kVersion), version, kVersion);  // 3.0.5-beta.1
    LOG_INFO("update: %s %s - the player is asked", version.c_str(),
             back ? "is the release channel's, chosen over this beta" : "is out");
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
    const int slot = wlog::open(layout::join(dir, layout::now(layout::File::DriverLog)), s.config.logLevel);
    installCrashHandler(dir);  // 0.52: where a crash happened, and a minidump
    LOG_INFO("uaRO Graphics %s in %s", kVersion, modulePath(GetModuleHandleA(nullptr)).c_str());
    if (slot > 1)  // 3.0.3-beta.1: the logs of a game started while another runs
        LOG_INFO("log: game window %d - another game runs beside it; its logs end in -%d (uaro-render64-%d.log, "
                 "uaro-pad-%d.log)", slot, slot, slot, slot);
    for (const std::string& line : moved) LOG_INFO("%s", line.c_str());
    // 3.0.9-beta.7 (the owner, R-3FS6YX): the old cards' 75 % of the automatic setup back to 100 once, before anything
    // reads the world's share (the settings app does the same when it opens)
    if (haveIni)
        if (const std::vector<settings::IniEdit> edits = settings::worldScaleReset(settings::readIni(ini));
            !edits.empty()) {
            const bool written = editIniFile(iniPath, edits);
            if (s.config.worldScale < 100)
                LOG_INFO("settings: [quality] world_scale %d -> 100 once, the world whole for everyone (3.0.9)%s",
                         s.config.worldScale, written ? "" : " - uaro-graphics.ini not written: this game only");
            else if (!written)
                LOG_ERROR("settings: uaro-graphics.ini could not be written ([utility] world_scale_reset)");
            s.config.worldScale = 100;
        }
    LOG_INFO("%s", padMountedText());  // 2.9
    logRoext();                         // 3.0.1-beta.8
    logUpdates();                       // 3.0.2-beta.5
    for (const char* folder : layout::kMadeAtStart) makeFolders(layout::join(dir, folder));
    safeModeFirst(dir, s.config);  // 2.12: before anything reads the renderer's settings
    {  // 2.12: the start panel in the player's language - the settings app's choice, else Windows'
        const auto keys = settings::readIni(ini);
        const auto chosen = keys.find("utility.language");
        s.lang = settings::windowLang(chosen == keys.end() ? std::nullopt : settings::parseLang(chosen->second),
                                      PRIMARYLANGID(GetUserDefaultUILanguage()));
        startLanguage(s.lang);
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
    // 3.0.5-beta.7: the settings app's «Тест графіки» started it - the test on its first map (win/context_test.cpp)
    s.graphicsTest = takeMark(graphics_test::kStartMark);
    if (s.graphicsTest)
        LOG_INFO("test: «Тест графіки» asked by the settings app - it begins %.0f s after a map shows",
                 graphics_test::kWaitS);
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
    s.memoryLevel = s.config.weakCard ? 1 : effect_groups::cardLevel(s.choice.api == Api::D3d12 && facts.d3d12, vramMb);
    // 3.0.4-beta.35: a weak card judged by how it draws (core/card_speed.hpp) - its verdict from the last games
    std::string judged;
    readTextFile(dir + "\\uaro-graphics\\card-level.txt", judged);
    s.cardLevel = card_speed::levelAt(s.memoryLevel, s.adapter, judged);
    s.cardDecided = card_speed::decided(s.memoryLevel, s.adapter, judged);
    LOG_INFO("effects: set %s, the card's level %d%s", effect_groups::presetToken(s.config.effects.preset),
             s.cardLevel,
             s.cardDecided ? (s.cardLevel > s.memoryLevel ? " (judged by its speed: it keeps up)"
                                                         : " (judged by its speed: the medium set too much)")
             : s.memoryLevel == 1 ? " (its speed to be judged on the maps)"
                                   : "");
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
