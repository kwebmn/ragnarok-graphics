#include "app/report_send.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>

#include "app/app_disk.hpp"
#include "app/download.hpp"
#include "app/files.hpp"
#include "app/reference_pack.hpp"
#include "app/reports.hpp"
#include "app/text.hpp"
#include "app/update_run.hpp"
#include "core/config.hpp"
#include "core/log_files.hpp"
#include "core/reports.hpp"
#include "core/settings/log_digest.hpp"
#include "core/usage.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr long long kAutoEvery = 6 * 3600;  // an automatic report at most once in 6 hours
constexpr DWORD kAnswersEveryMs = 10 * 60 * 1000;  // 3.0.9-beta.4: the developer's answers asked again while it runs
constexpr DWORD kOnlineEveryMs = 2 * 60 * 1000;    // 3.0.11-beta.10: the players on the maps (the site's «Онлайн»)

std::string hresult(long hr) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%08lX", static_cast<unsigned long>(hr));
    return buf;
}

// A file of the game's folder for the report: its end, the Windows user name hidden; "" when there is none.
std::string fileFor(const std::wstring& path, size_t most) {
    std::string text;
    if (!readFile(path, text)) return "";
    return maskUserPaths(report::tailOf(text, most));
}

}  // namespace

ReportFacts reportFacts(const Model& m, const std::vector<std::string>& problems) {
    const SystemInfo& sys = m.system();
    const Snapshot& s = m.now();
    ReportFacts f;
    f.utilityVersion = kVersion;
    f.driverVersion = s.installedVersion;
    f.windows = num(long(sys.windows.major)) + "." + num(long(sys.windows.minor)) + " build " +
                num(long(sys.windows.build));
    f.processor = processorText(sys.computer);  // 2.5.15
    f.memory = memoryText(sys.computer);
    f.power = powerText(sys.computer);
    f.gpu = sys.d3d12Probed ? sys.d3d12.adapter + ", " + vramText(sys.d3d12.vramMb) + ", driver " +
                                  (sys.gpuDriver.empty() ? "unknown" : sys.gpuDriver) +
                                  ", Direct3D 12: " + (sys.d3d12.ok ? "OK" : "failed " + hresult(sys.d3d12.hr))
                            : "not checked (Windows before 10)";
    // 0.51: every card (0.50.1: the renderer takes the fast one, 0.50.2: or the chosen one)
    for (const win::AdapterProbe& a : sys.adapters)
        f.gpu += "; card: " + a.name + ", " + vramText(a.vramMb) + ", Direct3D 12 " + (a.d3d12 ? "yes" : "no") +
                 ", Direct3D 11 " + (a.d3d11Level ? win::featureLevelName(a.d3d11Level) : std::string("no"));
    for (const MonitorInfo& mon : sys.monitors)
        f.monitors.push_back(num(mon.pixels.width) + "x" + num(mon.pixels.height) + ", " + num(mon.refresh) +
                             " Hz, " + num(mon.scale) + " %, HDR " + (mon.hdr ? "yes" : "no") +
                             (mon.primary ? ", main" : ""));
    f.iniText = s.iniText;
    const ScreenNow now = m.screenNow();
    f.screen = now.choice.show == Show::Window
                   ? "window " + num(now.choice.window.width) + "x" + num(now.choice.window.height) + ", interface " +
                         num(now.choice.ui) + " %, game " + num(now.game.width) + "x" + num(now.game.height)
                   : "full screen, game " + num(now.game.width) + "x" + num(now.game.height) + ", interface " +
                         num(now.choice.ui) + " %";
    if (now.roextOverride) f.screen += " (ROExt sets the window)";
    for (const ModuleFile& mod : s.modules)
        f.inputModules.push_back(utf8(mod.file) + ": " +
                                 (mod.info.kind == ModuleKind::Roext     ? "ROExt (" + mod.info.ini + ")"
                                  : mod.info.kind == ModuleKind::UaroPad ? std::string("uaRO Pad")
                                                                         : std::string("unknown")) +
                                 (mod.loaded ? "" : ", off"));
    for (const CompatFlag& c : s.compat)
        f.compatFlags.push_back(std::string(c.allUsers ? "HKLM: " : "HKCU: ") + c.flag);
    f.problems = problems;
    f.log = s.log;
    return f;
}

report::Upload makeUpload(const Model& m, const std::string& reason, bool automatic, const std::string& comment,
                          const std::vector<std::string>& problems, int logSlot) {
    const SystemInfo& sys = m.system();
    report::Upload u;
    u.reason = reason;
    u.automatic = automatic;
    u.version = kVersion;
    u.comment = comment;
    u.computer = computerKey(m.paths().dir);  // 2.13: the report comes back to «Звернення» of this computer
    const auto slotted = [](const std::wstring& path, int slot) { return wide(logfiles::slotted(utf8(path), slot)); };
    const std::wstring gameLog = slotted(m.paths().log, logSlot);  // 3.0.3-beta.1: that window's
    std::string log;
    readFile(gameLog, log);
    // 2.6.12: the settings and the log as the disk has them now - the game's console writes the ini while this app
    // keeps what it read when it opened (R-TDWB4X and R-B27V4Y carried R-MUQ8U5's text, their files fresh)
    ReportFacts facts = reportFacts(m, problems);
    std::string ini;
    if (readFile(m.paths().ini, ini)) facts.iniText = ini;
    if (!log.empty()) facts.log = settings::digestLog(log);
    u.report = makeReport(facts);
    u.windows = num(long(sys.windows.major)) + "." + num(long(sys.windows.minor)) + " build " +
                num(long(sys.windows.build));
    u.gpu = !sys.adapters.empty() ? sys.adapters.front().name : sys.d3d12.adapter;
    const std::string chosen = m.now().config.gpu;  // 0.50.2
    for (const win::AdapterProbe& a : sys.adapters)
        if (!chosen.empty() && a.name == chosen) u.gpu = a.name;
    u.api = report::apiOfLog(log);
    const std::wstring& dir = m.paths().dir;
    // 2.8: the logs in uaro-graphics\logs, the previous game's too; the names the site knows them by are 2.x's
    struct Attached {
        const char* name;
        std::wstring path;
        size_t most;
    };
    const Attached files[] = {
        {"uaro-graphics.log", gameLog, 1u << 20},
        {"uaro-graphics.prev.log", wide(logfiles::previous(utf8(gameLog))), 512u << 10},
        {"uaro-render64.log", slotted(ourPlace(dir, layout::now(layout::File::Render64Log)), logSlot), 512u << 10},
        {"uaro-pack.log", ourPlace(dir, layout::now(layout::File::PackLog)), 256u << 10},
        {"uaro-pad.log", slotted(ourPlace(dir, layout::now(layout::File::PadLog)), logSlot), 256u << 10},  // 2.9
        {"uaro-graphics.ini", m.paths().ini, 256u << 10}};
    for (const Attached& f : files) {
        const std::string text = fileFor(f.path, f.most);
        if (!text.empty()) u.files.emplace_back(f.name, text);
    }
    // 3.0.3-beta.1: the other game windows' logs - of games running beside it, or just over (12 hours)
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    const auto ticks = [](const FILETIME& t) { return (uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime; };
    for (int slot = 1; slot <= logfiles::kSlots && u.files.size() < report::kMostFiles; ++slot) {
        const std::wstring other = slotted(m.paths().log, slot);
        WIN32_FILE_ATTRIBUTE_DATA a{};
        if (slot == logSlot || !GetFileAttributesExW(other.c_str(), GetFileExInfoStandard, &a) ||
            ticks(now) - ticks(a.ftLastWriteTime) > 12ull * 3600 * 10000000ull)
            continue;
        const std::string text = fileFor(other, 256u << 10);
        if (!text.empty()) u.files.emplace_back("uaro-graphics-" + std::to_string(slot) + ".log", text);
    }
    // 0.52: the pictures the pack had none of (the driver saves the first 64 of a game, uaro-graphics\missing)
    const std::wstring missing = dir + L"\\uaro-graphics\\missing";
    for (const std::wstring& name : listFolder(missing, false)) {
        if (u.images.size() == report::kMostImages) break;
        std::string bytes;
        const bool bmp = name.size() > 4 && name.compare(name.size() - 4, 4, L".bmp") == 0;
        if (bmp && readFile(missing + L"\\" + name, bytes))
            u.images.emplace_back(utf8(name), bytes);
    }
    // 0.52: a crash's minidump (win/crash.cpp) of the last week
    const std::wstring dump = dir + L"\\uaro-graphics\\crash.dmp";
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (GetFileAttributesExW(dump.c_str(), GetFileExInfoStandard, &a)) {
        FILETIME now{};
        GetSystemTimeAsFileTime(&now);
        const auto ticks = [](const FILETIME& t) { return (static_cast<unsigned long long>(t.dwHighDateTime) << 32) |
                                                          t.dwLowDateTime; };
        if (ticks(now) - ticks(a.ftLastWriteTime) < 7ull * 24 * 3600 * 10000000ull) readFile(dump, u.dump);
    }
    return u;
}

namespace {

// A JSON body POSTed to uaro.kiev.ua's `path` (2.13: app/download's postPath): its status and answer, false when no
// answer came (`error` says why).
bool postJson(const wchar_t* path, const std::string& body, DWORD& status, std::string& answer, std::string& error) {
    long code = 0;
    const bool answered = postPath(utf8(path), body, code, answer, error);
    status = DWORD(code);
    return answered;
}

}  // namespace

namespace {

unsigned long long sizeOf(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a)) return 0;
    return (static_cast<unsigned long long>(a.nFileSizeHigh) << 32) | a.nFileSizeLow;
}

}  // namespace

bool sendUsage(const std::wstring& dir, bool force, std::string& error, size_t* lines) {
    if (lines) *lines = 0;
    std::string ini;
    readFile(ourFile(dir, layout::File::Ini), ini);
    const Config c = parseConfig(ini);
    if (!c.statsSend || c.statsId.empty()) return false;  // no leave: nothing goes
    HANDLE lock = CreateMutexA(nullptr, FALSE, "Local\\uaro-graphics-stats");
    if (!lock) return true;
    const DWORD got = WaitForSingleObject(lock, 0);
    if (got != WAIT_OBJECT_0 && got != WAIT_ABANDONED) {
        CloseHandle(lock);
        return true;  // the other one sends them
    }
    struct Unlock {
        HANDLE h;
        ~Unlock() {
            ReleaseMutex(h);
            CloseHandle(h);
        }
    } unlock{lock};
    const std::wstring folder = dir + L"\\uaro-graphics";
    const std::wstring file = folder + L"\\usage.jsonl", sending = folder + L"\\usage-sending.jsonl";
    const std::wstring sentAt = folder + L"\\stats-sent.txt";
    std::string stamp, local, text;
    readFile(sentAt, stamp);
    long long then = 0;
    usage::readSentStamp(stamp, then, local);
    const long long now = static_cast<long long>(std::time(nullptr));
    if (!readFile(sending, text) || text.empty()) {  // none left from a send cut short: the game's, when due
        if (!usage::due(now, then, size_t(std::min<unsigned long long>(sizeOf(file), 1u << 30)), force)) return true;
        // taken whole: the game only adds to it, opening it for each line, and starts a new one from now on
        if (!MoveFileExW(file.c_str(), sending.c_str(), 0)) return true;  // being written this moment: next time
        if (!readFile(sending, text) || text.empty()) {
            DeleteFileW(sending.c_str());
            return true;
        }
    }
    for (size_t at = 0; at < text.size();) {
        size_t taken = 0;
        const std::string body = usage::uploadBody(c.statsId, kVersion, text.substr(at), usage::kMostUpload, taken);
        if (!taken) break;  // the rest no whole line (a game cut off in the middle of one): it goes with the file
        DWORD status = 0;
        std::string answer;
        const bool answered = postJson(L"/api/graphics/stats", body, status, answer, error);
        if (answered && status >= 400 && status < 500 && status != 408 && status != 429) {
            at += taken;  // refused for good (not a busy or a down site): it would never go, the rest would wait
            continue;
        }
        if (!answered || (status != 200 && status != 201)) {
            if (answered) error = "uaro.kiev.ua answered " + std::to_string(status);
            if (at) writeFileAtomic(sending, text.substr(at));  // what went leaves the file
            return false;
        }
        if (lines) *lines += size_t(std::count(text.begin() + std::ptrdiff_t(at),
                                               text.begin() + std::ptrdiff_t(at + taken), '\n'));
        at += taken;
    }
    DeleteFileW(sending.c_str());
    SYSTEMTIME t{};
    GetLocalTime(&t);
    char when[64];
    std::snprintf(when, sizeof when, "%04u-%02u-%02u %02u:%02u", unsigned(t.wYear), unsigned(t.wMonth),
                  unsigned(t.wDay), unsigned(t.wHour), unsigned(t.wMinute));
    writeFileAtomic(sentAt, usage::sentStamp(now, when));
    return true;
}

void usageState(const std::wstring& dir, std::string& lastSent, unsigned long long& waiting) {
    const std::wstring folder = dir + L"\\uaro-graphics";
    std::string stamp;
    readFile(folder + L"\\stats-sent.txt", stamp);
    long long seconds = 0;
    usage::readSentStamp(stamp, seconds, lastSent);
    if (lastSent.empty() && seconds > 0) lastSent = usage::utcText(seconds);  // 2.2.1 wrote the seconds alone
    waiting = sizeOf(folder + L"\\usage.jsonl") + sizeOf(folder + L"\\usage-sending.jsonl");
}

int watchGame(unsigned long pid, int logSlot) {
    // 2.10: the update check of every game's start: uaro-graphics\update.ini for the next start's question
    checkNow(exeFolder());
    // 3.0.9-beta.4: the developer's answers for the game's card - at its start, then every 10 minutes while it runs;
    // only on a computer that sent a report (no key: no question)
    const bool answers = !existingComputerKey(exeFolder()).empty();
    if (answers) noteAnswers(exeFolder());
    std::string ini;
    readFile(ourFile(exeFolder(), layout::File::Ini), ini);
    const Config config = parseConfig(ini);
    // 3.0.11-beta.5: the game's reference pack, the new interface on (3.0.11-beta.7: or «Довідник») - only when the
    // site has another one
    // 3.0.11-beta.10: and the players on each map now, every 2 minutes while the game runs (core/reference_live)
    const bool live = config.newMinimap || config.referenceWindow;
    if (live) {
        std::string error;
        noteReference(exeFolder(), error);
        noteWorld(exeFolder(), error);   // 3.0.11-beta.11
        noteQuests(exeFolder(), error);  // 3.0.11-beta.16
        noteOnline(exeFolder(), error);
        noteLive(exeFolder(), error);  // 3.0.11-beta.12
    }
    if (!config.reportAuto && !config.statsSend && !answers && !live) return 0;  // nothing more to watch for
    HANDLE game = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!game) return 0;
    ULONGLONG answered = GetTickCount64();
    while (WaitForSingleObject(game, live ? kOnlineEveryMs : answers ? kAnswersEveryMs : INFINITE) == WAIT_TIMEOUT) {
        if (live) {
            std::string error;
            noteOnline(exeFolder(), error);
            noteLive(exeFolder(), error);
        }
        if (answers && GetTickCount64() - answered >= kAnswersEveryMs) {
            answered = GetTickCount64();
            noteAnswers(exeFolder());
        }
    }
    CloseHandle(game);
    if (!config.reportAuto && !config.statsSend) return 0;
    Sleep(2000);  // the log closed and flushed
    std::string ignored;
    sendUsage(exeFolder(), false, ignored);  // 2.2.1: the statistics, with the player's leave
    std::string log;  // 3.0.3-beta.1: that game window's
    const std::wstring first = ourPlace(exeFolder(), layout::now(layout::File::DriverLog));
    if (!readFile(wide(logfiles::slotted(utf8(first), logSlot)), log)) return 0;
    const std::string reason = report::reasonOfLog(log);
    if (reason.empty()) return 0;
    Model m;
    if (!m.now().config.reportAuto) return 0;  // turned off meanwhile
    if (std::time(nullptr) - lastAutomaticIn(m.paths().dir) < kAutoEvery) return 0;
    // 2.13: kept for later when the site does not answer (app/reports), and in the tester's «Звернення»
    const NewReport r = sendNew(m.paths().dir, makeUpload(m, reason, true, "", {}, logSlot));
    return r.code.empty() && !r.queued ? 1 : 0;
}

int sendTestReport(const std::wstring& file, int logSlot) {
    const std::wstring answer = file + L".sent";
    std::string table;
    if (!readFile(file, table) || table.empty()) {
        writeFileAtomic(answer, "error: the test's result is missing");
        return 1;
    }
    if (table.size() > report::kMostComment) {  // the site's words at most, whole lines
        table.resize(report::kMostComment);
        table.resize(table.rfind('\n') == std::string::npos ? table.size() : table.rfind('\n') + 1);
    }
    Model m;
    report::Upload u = makeUpload(m, "test", false, table, {}, logSlot);
    u.kind = reports::kindToken(reports::Kind::Test);
    const NewReport r = sendNew(m.paths().dir, u);
    const std::string said = !r.code.empty() ? r.code
                             : r.queued      ? "error: " + (r.error.empty() ? std::string("no answer") : r.error) +
                                              " - kept, it goes with the next report"
                                             : "error: " + (r.error.empty() ? std::string("refused") : r.error);
    writeFileAtomic(answer, said);
    return r.code.empty() ? 1 : 0;
}

}  // namespace uagfx::app
