#include "app/report_send.hpp"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>

#include "app/app_disk.hpp"
#include "app/files.hpp"
#include "app/shots.hpp"
#include "app/text.hpp"
#include "core/log_files.hpp"
#include "core/settings/log_digest.hpp"
#include "core/usage.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr long long kAutoEvery = 6 * 3600;  // an automatic report at most once in 6 hours

std::string hresult(long hr) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%08lX", static_cast<unsigned long>(hr));
    return buf;
}

std::wstring sentFile(const Model& m) { return m.paths().dir + L"\\uaro-graphics\\reports.txt"; }

struct Sent {
    long long time = 0;
    std::string code, reason;
    bool automatic = false;
};

std::vector<Sent> readSent(const Model& m) {  // oldest first
    std::vector<Sent> out;
    std::string text;
    if (!readFile(sentFile(m), text)) return out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream l(line);
        Sent s;
        std::string kind;
        if (l >> s.time >> s.code >> s.reason >> kind) {
            s.automatic = kind == "auto";
            out.push_back(s);
        }
    }
    return out;
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
                          const std::vector<std::string>& problems, bool shots) {
    const SystemInfo& sys = m.system();
    report::Upload u;
    u.reason = reason;
    u.automatic = automatic;
    u.version = kVersion;
    u.comment = comment;
    std::string log;
    readFile(m.paths().log, log);
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
        {"uaro-graphics.log", m.paths().log, 1u << 20},
        {"uaro-graphics.prev.log", wide(logfiles::previous(utf8(m.paths().log))), 512u << 10},
        {"uaro-render64.log", ourPlace(dir, layout::now(layout::File::Render64Log)), 512u << 10},
        {"uaro-pack.log", ourPlace(dir, layout::now(layout::File::PackLog)), 256u << 10},
        {"uaro-graphics.ini", m.paths().ini, 256u << 10}};
    for (const Attached& f : files) {
        const std::string text = fileFor(f.path, f.most);
        if (!text.empty()) u.files.emplace_back(f.name, text);
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
    if (shots) u.shots = recentShots(shotFolders(dir), report::kMostShots);  // 1.8.1
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

// A JSON body POSTed to uaro.kiev.ua's `path`: its status and answer, false when no answer came (`error` says why).
bool postJson(const wchar_t* path, const std::string& body, DWORD& status, std::string& answer, std::string& error) {
    HINTERNET session = WinHttpOpen(L"uaRO Graphics Settings", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) WinHttpSetTimeouts(session, 10000, 10000, 30000, 30000);
    HINTERNET connect = session ? WinHttpConnect(session, report::kHost, INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr;
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"POST", path, nullptr, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                                : nullptr;
    const wchar_t* headers = L"Content-Type: application/json\r\nAccept: application/json\r\n";
    status = 0;
    DWORD size = sizeof status;
    const bool sent = request &&
                      WinHttpSendRequest(request, headers, DWORD(-1L), const_cast<char*>(body.data()),
                                         DWORD(body.size()), DWORD(body.size()), 0) &&
                      WinHttpReceiveResponse(request, nullptr) &&
                      WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                          WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    answer.clear();
    if (sent) {
        DWORD n = 0;
        while (WinHttpQueryDataAvailable(request, &n) && n && answer.size() < (64u << 10)) {
            std::string part(n, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, &part[0], n, &read) || !read) break;
            answer.append(part, 0, read);
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    if (!sent) {
        error = "no answer from uaro.kiev.ua (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    return true;
}

}  // namespace

bool sendUpload(const report::Upload& u, std::string& code, std::string& error) {
    DWORD status = 0;
    std::string answer;
    if (!postJson(report::kPath, report::uploadJson(u), status, answer, error)) return false;
    if (status == 429) {
        error = "too many reports from this computer - try again in an hour";
        return false;
    }
    if (status != 201 || !report::codeOf(answer, code)) {
        error = "uaro.kiev.ua answered " + std::to_string(status);
        return false;
    }
    return true;
}

void noteSent(const Model& m, const std::string& code, const std::string& reason, bool automatic) {
    DeleteFileW((m.paths().dir + L"\\uaro-graphics\\crash.dmp").c_str());  // 0.52: on the site now
    std::string text;
    readFile(sentFile(m), text);
    text += std::to_string(static_cast<long long>(std::time(nullptr))) + " " + code + " " + reason + " " +
            (automatic ? "auto" : "manual") + "\n";
    makeFolder(m.paths().dir + L"\\uaro-graphics");
    writeFileAtomic(sentFile(m), report::tailOf(text, 16u << 10));
}

std::vector<std::string> sentCodes(const Model& m, size_t most) {
    const std::vector<Sent> all = readSent(m);
    std::vector<std::string> out;
    for (auto it = all.rbegin(); it != all.rend() && out.size() < most; ++it) out.push_back(it->code);
    return out;
}

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

int watchGame(unsigned long pid) {
    HANDLE game = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!game) return 0;
    WaitForSingleObject(game, INFINITE);
    CloseHandle(game);
    Sleep(2000);  // the log closed and flushed
    std::string ignored;
    sendUsage(exeFolder(), false, ignored);  // 2.2.1: the statistics, with the player's leave
    std::string log;
    if (!readFile(ourPlace(exeFolder(), layout::now(layout::File::DriverLog)), log)) return 0;
    const std::string reason = report::reasonOfLog(log);
    if (reason.empty()) return 0;
    Model m;
    if (!m.now().config.reportAuto) return 0;  // turned off meanwhile
    for (const Sent& s : readSent(m))
        if (s.automatic && std::time(nullptr) - s.time < kAutoEvery) return 0;
    std::string code, error;
    if (!sendUpload(makeUpload(m, reason, true, "", {}), code, error)) return 1;
    noteSent(m, code, reason, true);
    return 0;
}

}  // namespace uagfx::app
