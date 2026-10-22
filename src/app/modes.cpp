#include "app/modes.hpp"

#include <shlobj.h>

#include "app/app_disk.hpp"
#include "app/files.hpp"
#include "app/installer.hpp"
#include "app/model.hpp"
#include "app/system.hpp"
#include "app/text.hpp"
#include "app/update_run.hpp"
#include "app/update_window.hpp"
#include "core/layout.hpp"
#include "core/settings/ini_edit.hpp"

namespace uagfx::app {
namespace {

using settings::Text;

std::wstring selfPath() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

void waitFor(unsigned long pid) {  // the settings app that started this, or the game: up to a minute
    if (!pid) return;
    if (HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, pid)) {
        WaitForSingleObject(p, 60000);
        CloseHandle(p);
    }
}

bool waitForGame(const std::wstring& dir, const Report& report, std::wstring& error) {
    while (gameRunning(dir + L"\\uaRO.exe")) {
        Step s;
        s.what = Step::Wait;
        if (!report(s)) {
            error = L"stopped";
            return false;
        }
        waitMs(500);
    }
    return true;
}

void then(const std::wstring& dir, const Args& a) {
    if (a.then == Args::Then::App) launch(dir + L"\\uaro-graphics.exe", dir);
    if (a.then == Args::Then::Game) {
        markOurStart();  // 2.11: the driver plays at once, no launcher «Гра» again
        launchArgs(dir + L"\\uaRO.exe", a.gameArgs, dir, false);
    }
}

void failed(settings::Lang l, Text t, const std::wstring& error) {
    if (error != L"stopped") tell(wtrf(l, t, {utf8(error)}), true);
}

// install.bat's (an older update.bat put the package's files into uaro-graphics\): the root's copies of ddraw.dll and
// uaro-graphics.exe - no rollback point of a half - and what install.bat did; the old .bat files stay while that
// update.bat runs (the settings app's next start takes them).
int install(const std::wstring& dir, const Args& a, settings::Lang l) {
    const std::wstring work = dir + L"\\" + wide(layout::kUpdateWork);
    const std::wstring staged = work + L"\\files";
    removeTree(staged);
    makeFolder(work);
    makeFolder(staged);
    makeFolder(staged + L"\\uaro-graphics");
    for (const wchar_t* name : {L"ddraw.dll", L"uaro-graphics.exe"}) {
        std::string bytes;
        if (readFile(dir + L"\\uaro-graphics\\" + name, bytes))
            writeFileAtomic(staged + L"\\uaro-graphics\\" + name, bytes);
    }
    const installer::Result put = installer::putPackage(dir, staged, true);
    removeTree(work);
    if (!put.ok) {
        if (!a.quiet) failed(l, Text::UpdateNothingChanged, put.error);
        return 1;
    }
    installer::finishInstall(dir, false);
    if (!a.quiet) {
        tell(wtrf(l, Text::InstallDone, {installedVersion(dir)}), false);
        launch(dir + L"\\uaro-graphics.exe", dir);
    }
    return 0;
}

int update(const std::wstring& dir, const Args& a, settings::Lang l) {
    waitFor(a.pid);
    const bool repair = a.mode == Args::Mode::Repair;
    const CheckResult c = checkNow(dir);
    std::wstring error = wide(c.error);
    bool ok = false;
    if (c.ok) {
        const std::wstring title =
            repair ? wtr(l, Text::Repairing) : wtrf(l, Text::UpdatingTo, {c.site.package.version});
        ok = runWithProgress(nullptr, title, l, a.then == Args::Then::Game,
                             [&](const Report& report, std::wstring& e) {
                                 return runUpdate(dir, c.site, repair, report, e);
                             },
                             error);
    }
    if (!ok) failed(l, Text::UpdateNothingChanged, error);
    then(dir, a);
    return ok ? 0 : 1;
}

int rollback(const std::wstring& dir, const Args& a, settings::Lang l) {
    waitFor(a.pid);
    const std::string from = installedVersion(dir), to = installer::previousVersion(dir);
    std::wstring error;
    const bool ok = runWithProgress(nullptr, wtrf(l, Text::RollingBack, {to}), l, false,
                                    [&](const Report& report, std::wstring& e) {
                                        if (!waitForGame(dir, report, e)) return false;
                                        const installer::Result r = installer::rollBack(dir);
                                        if (!r.ok) e = r.error;
                                        if (r.ok) setSkipped(dir, from);  // not offered again: it went wrong
                                        return r.ok;
                                    },
                                    error);
    if (!ok) failed(l, Text::UpdateNothingChanged, error);
    then(dir, a);
    return ok ? 0 : 1;
}

// The removal's first half: the question, then this exe from a copy in %TEMP% (it is removed too).
int uninstallAsk(const std::wstring& dir, settings::Lang l) {
    bool keep = true;
    if (!askUninstall(dir, l, keep)) return 0;
    wchar_t temp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp);
    const std::wstring copy = std::wstring(temp) + L"uaro-graphics-uninstall.exe";
    Args run;
    run.mode = Args::Mode::UninstallRun;
    run.dir = dir;
    run.keep = keep;
    run.pid = GetCurrentProcessId();
    const bool admin = !folderWritable(dir) && !elevatedNow();
    if (!CopyFileW(selfPath().c_str(), copy.c_str(), FALSE) || launchArgs(copy, argsLine(run), temp, admin) != 0) {
        failed(l, Text::UninstallFailed, L"Windows did not start " + copy);
        return 1;
    }
    return 0;
}

int uninstallRun(const std::wstring& dir, const Args& a, settings::Lang l) {
    waitFor(a.pid);
    std::wstring error;
    const bool ok = runWithProgress(nullptr, wtr(l, Text::Uninstalling), l, false,
                                    [&](const Report& report, std::wstring& e) {
                                        if (!waitForGame(dir, report, e)) return false;
                                        const installer::Result r = installer::uninstall(dir, a.keep);
                                        if (!r.ok) e = r.error;
                                        return r.ok;
                                    },
                                    error);
    if (ok)
        tell(wtr(l, Text::UninstallDone) + (a.keep ? L"\n" + wtr(l, Text::UninstallKept) : L""), false);
    else
        failed(l, Text::UninstallFailed, error);
    MoveFileExW(selfPath().c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);  // this copy in %TEMP%, as administrator
    return ok ? 0 : 1;
}

// The game's question before its login window (the driver waits for this process): "Update" starts the update to
// wait for the game, which the driver then closes (kUpdateChosen), and to start it again.
int askAtGameStart(const std::wstring& dir, const Args& a, settings::Lang l) {
    std::string info, version;
    readFile(dir + L"\\" + wide(layout::kUpdateInfo), info);
    if (!update::askAtStart(info, installedVersion(dir), version)) return 0;
    if (askUpdate(version, l) != Answer::Update) return 0;
    Args run;
    run.mode = Args::Mode::Update;
    run.pid = a.pid;  // the game
    run.then = Args::Then::Game;
    run.gameArgs = a.gameArgs;
    const bool admin = !folderWritable(dir) && !elevatedNow();
    if (launchArgs(selfPath(), argsLine(run), dir, admin) != 0) return 0;  // no update: play
    return int(update::kUpdateChosen);
}

// This exe's own package (the setup's): where it is, its bytes.
bool packageOf(uint64_t& offset, uint64_t& size) {
    HANDLE f = CreateFileW(selfPath().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER total{}, at{};
    uint8_t tail[16] = {};
    DWORD got = 0;
    bool ok = GetFileSizeEx(f, &total) && total.QuadPart > 16;
    at.QuadPart = total.QuadPart - 16;
    ok = ok && SetFilePointerEx(f, at, nullptr, FILE_BEGIN) && ReadFile(f, tail, 16, &got, nullptr) && got == 16;
    CloseHandle(f);
    return ok && update::payloadAt(tail, uint64_t(total.QuadPart), offset, size);
}

bool readPackage(uint64_t offset, uint64_t size, std::string& out) {
    HANDLE f = CreateFileW(selfPath().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER at{};
    at.QuadPart = LONGLONG(offset);
    out.assign(size_t(size), '\0');
    DWORD got = 0;
    const bool ok = SetFilePointerEx(f, at, nullptr, FILE_BEGIN) && ReadFile(f, &out[0], DWORD(size), &got, nullptr) &&
                    got == size;
    CloseHandle(f);
    return ok;
}

std::wstring chooseFolder(settings::Lang l) {
    const std::wstring title = wtr(l, Text::SetupChoose);
    BROWSEINFOW bi{};
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE id = SHBrowseForFolderW(&bi);
    if (!id) return L"";
    wchar_t path[MAX_PATH] = {};
    SHGetPathFromIDListW(id, path);
    CoTaskMemFree(id);
    return path;
}

// uaro-graphics-setup.exe: the game's folder (its own when uaRO.exe is there, else the player's choice), the package
// it carries put in as an update's, then the installed settings app.
int setup(const Args& a) {
    const settings::Lang first = settings::windowLang(std::nullopt, PRIMARYLANGID(GetUserDefaultUILanguage()));
    uint64_t offset = 0, size = 0;
    if (!packageOf(offset, size)) {
        tell(wtr(first, Text::SetupDamaged), true);
        return 1;
    }
    std::wstring dir = !a.dir.empty() ? a.dir : exeFolder();
    if (!fileExists(dir + L"\\uaRO.exe")) {
        dir = chooseFolder(first);
        if (dir.empty()) return 0;
        if (!fileExists(dir + L"\\uaRO.exe")) {
            tell(wtr(first, Text::SetupNoGame), true);
            return 1;
        }
    }
    const settings::Lang l = playerLang(dir);
    if (!folderWritable(dir) && !elevatedNow()) {  // a game under Program Files: the setup again as administrator
        Args again = a;
        again.dir = dir;
        if (launchArgs(selfPath(), argsLine(again), dir, true) == 0) return 0;
        failed(l, Text::UpdateNothingChanged, L"Windows did not give the administrator rights the game's folder needs");
        return 1;
    }
    std::wstring error;
    const bool ok = runWithProgress(nullptr, wtr(l, Text::SetupTitle), l, false,
                                    [&](const Report& report, std::wstring& e) {
                                        std::string zip;
                                        if (!readPackage(offset, size, zip)) {
                                            e = wtr(l, Text::SetupDamaged);
                                            return false;
                                        }
                                        return runInstall(dir, std::move(zip), report, e);
                                    },
                                    error);
    if (!ok) {
        failed(l, Text::UpdateNothingChanged, error);
        return 1;
    }
    tell(wtrf(l, Text::InstallDone, {installedVersion(dir)}), false);
    launch(dir + L"\\uaro-graphics.exe", dir);
    return 0;
}

bool writes(Args::Mode m) {
    return m == Args::Mode::Install || m == Args::Mode::Update || m == Args::Mode::Repair ||
           m == Args::Mode::Rollback || m == Args::Mode::UninstallRun;
}

}  // namespace

bool carriesPackage() {
    uint64_t offset = 0, size = 0;
    return packageOf(offset, size);
}

std::wstring gameFolder() {
    const std::wstring own = exeFolder();
    if (fileExists(own + L"\\uaRO.exe")) return own;
    const size_t slash = own.find_last_of(L'\\');
    const std::wstring up = slash == std::wstring::npos ? own : own.substr(0, slash);
    return fileExists(up + L"\\uaRO.exe") ? up : own;
}

settings::Lang playerLang(const std::wstring& dir) {
    std::string text;
    readFile(ourFile(dir, layout::File::Ini), text);
    const auto ini = settings::readIni(text);
    const auto it = ini.find("utility.language");
    return settings::windowLang(it == ini.end() ? std::nullopt : settings::parseLang(it->second),
                                PRIMARYLANGID(GetUserDefaultUILanguage()));
}

int runMode(const Args& a) {
    const std::wstring dir = a.mode == Args::Mode::UninstallRun ? a.dir : gameFolder();
    const settings::Lang l = playerLang(dir);
    if (writes(a.mode) && !folderWritable(dir) && !elevatedNow()) {  // a game under Program Files: as administrator
        if (launchArgs(selfPath(), argsLine(a), dir, true) == 0) return 0;
        failed(l, a.mode == Args::Mode::UninstallRun ? Text::UninstallFailed : Text::UpdateNothingChanged,
               L"Windows did not give the administrator rights the game's folder needs");
        return 1;
    }
    switch (a.mode) {
        case Args::Mode::Install: return install(dir, a, l);
        case Args::Mode::Update:
        case Args::Mode::Repair: return update(dir, a, l);
        case Args::Mode::Rollback: return rollback(dir, a, l);
        case Args::Mode::Uninstall: return uninstallAsk(dir, l);
        case Args::Mode::UninstallRun: return uninstallRun(dir, a, l);
        case Args::Mode::AskUpdate: return askAtGameStart(dir, a, l);
        case Args::Mode::Setup: return setup(a);
        default: return 0;
    }
}

std::string startMode(Args a) {
    const std::wstring dir = gameFolder();
    a.pid = GetCurrentProcessId();
    const bool admin = a.mode != Args::Mode::Uninstall && !folderWritable(dir) && !elevatedNow();
    const unsigned long e = launchArgs(selfPath(), argsLine(a), dir, admin);
    return e == 0 ? "" : "Windows did not start it (error " + std::to_string(e) + ")";
}

std::wstring playNow(Model& m, const std::string& offer, settings::Lang l, bool& closing) {
    closing = false;
    if (offer.empty()) return m.launchGame(l);
    Args a;
    a.mode = Args::Mode::Update;
    a.then = Args::Then::Game;
    const std::string err = startMode(a);
    if (!err.empty()) return wtrf(l, Text::UpdateError, {err});
    closing = true;
    return L"";
}

std::wstring getDlss(HWND owner, const std::wstring& dir, settings::Lang l) {
    std::wstring error;
    const bool ok = runWithProgress(owner, wtr(l, Text::DlssDownload), l, false,
                                    [&](const Report& report, std::wstring& e) {
                                        const CheckResult c = checkNow(dir);
                                        if (!c.ok) e = wide(c.error);
                                        return c.ok && runDlss(dir, c.site, report, e);
                                    },
                                    error);
    return ok ? L"" : error;
}

std::wstring getModels(HWND owner, const std::wstring& dir, settings::Lang l) {
    std::wstring error;
    const bool ok = runWithProgress(owner, wtr(l, Text::AiGetModels), l, false,
                                    [&](const Report& report, std::wstring& e) {
                                        const CheckResult c = checkNow(dir);
                                        if (!c.ok) e = wide(c.error);
                                        return c.ok && runModels(dir, c.site, report, e);
                                    },
                                    error);
    return ok ? L"" : error;
}

}  // namespace uagfx::app
