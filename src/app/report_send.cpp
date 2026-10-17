#include "app/report_send.hpp"

#include <windows.h>
#include <winhttp.h>

#include <cstdio>
#include <ctime>
#include <sstream>

#include "app/files.hpp"
#include "app/shots.hpp"
#include "app/text.hpp"
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
    u.report = makeReport(reportFacts(m, problems));
    u.windows = num(long(sys.windows.major)) + "." + num(long(sys.windows.minor)) + " build " +
                num(long(sys.windows.build));
    u.gpu = !sys.adapters.empty() ? sys.adapters.front().name : sys.d3d12.adapter;
    const std::string chosen = m.now().config.gpu;  // 0.50.2
    for (const win::AdapterProbe& a : sys.adapters)
        if (!chosen.empty() && a.name == chosen) u.gpu = a.name;
    std::string log;
    readFile(m.paths().log, log);
    u.api = report::apiOfLog(log);
    const std::wstring& dir = m.paths().dir;
    const std::pair<const wchar_t*, size_t> files[] = {
        {L"uaro-graphics.log", 1u << 20}, {L"uaro-render64.log", 512u << 10}, {L"uaro-pack.log", 256u << 10},
        {L"uaro-graphics.ini", 256u << 10}};
    for (const auto& f : files) {
        const std::string text = fileFor(dir + L"\\" + f.first, f.second);
        if (!text.empty()) u.files.emplace_back(utf8(f.first), text);
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

bool sendUpload(const report::Upload& u, std::string& code, std::string& error) {
    const std::string body = report::uploadJson(u);
    HINTERNET session = WinHttpOpen(L"uaRO Graphics Settings", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) WinHttpSetTimeouts(session, 10000, 10000, 30000, 30000);
    HINTERNET connect = session ? WinHttpConnect(session, report::kHost, INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr;
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"POST", report::kPath, nullptr, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                                : nullptr;
    const wchar_t* headers = L"Content-Type: application/json\r\nAccept: application/json\r\n";
    DWORD status = 0, size = sizeof status;
    const bool sent = request &&
                      WinHttpSendRequest(request, headers, DWORD(-1L), const_cast<char*>(body.data()),
                                         DWORD(body.size()), DWORD(body.size()), 0) &&
                      WinHttpReceiveResponse(request, nullptr) &&
                      WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                          WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    std::string answer;
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

int watchGame(unsigned long pid) {
    HANDLE game = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!game) return 0;
    WaitForSingleObject(game, INFINITE);
    CloseHandle(game);
    Sleep(2000);  // the log closed and flushed
    std::string log;
    if (!readFile(exeFolder() + L"\\uaro-graphics.log", log)) return 0;
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
