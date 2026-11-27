#include "app/mcp_watch.hpp"

#include <winsock2.h>
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "app/model.hpp"
#include "app/text.hpp"
#include "core/config.hpp"
#include "core/json.hpp"
#include "core/mcp.hpp"
#include "core/mcp_watch.hpp"
#include "core/report_upload.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/schema.hpp"
#include "core/version.hpp"

namespace uagfx::app::mcpwatch {
namespace {

// Winsock by name: the app imports none of it (tools/check_exe.py)
using SocketFn = SOCKET(WSAAPI*)(int, int, int);
using BindFn = int(WSAAPI*)(SOCKET, const sockaddr*, int);
using ListenFn = int(WSAAPI*)(SOCKET, int);
using AcceptFn = SOCKET(WSAAPI*)(SOCKET, sockaddr*, int*);
using RecvFn = int(WSAAPI*)(SOCKET, char*, int, int);
using SendFn = int(WSAAPI*)(SOCKET, const char*, int, int);
using CloseFn = int(WSAAPI*)(SOCKET);
using SelectFn = int(WSAAPI*)(int, fd_set*, fd_set*, fd_set*, const timeval*);
using SetOptFn = int(WSAAPI*)(SOCKET, int, int, const char*, int);
using StartupFn = int(WSAAPI*)(WORD, LPWSADATA);
using ConnectFn = int(WSAAPI*)(SOCKET, const sockaddr*, int);
struct Calls {
    SocketFn socket = nullptr;
    BindFn bind = nullptr;
    ListenFn listen = nullptr;
    AcceptFn accept = nullptr;
    RecvFn recv = nullptr;
    SendFn send = nullptr;
    CloseFn closesocket = nullptr;
    SelectFn select = nullptr;
    SetOptFn setsockopt = nullptr;
    StartupFn startup = nullptr;
    ConnectFn connect = nullptr;
    bool ok() const {
        return socket && bind && listen && accept && recv && send && closesocket && select && setsockopt && startup &&
               connect;
    }
};

Calls calls() {
    Calls c;
    const HMODULE ws = LoadLibraryW(L"ws2_32.dll");
    if (!ws) return c;
    const auto get = [&](const char* name) { return reinterpret_cast<void*>(GetProcAddress(ws, name)); };
    c.socket = reinterpret_cast<SocketFn>(get("socket"));
    c.bind = reinterpret_cast<BindFn>(get("bind"));
    c.listen = reinterpret_cast<ListenFn>(get("listen"));
    c.accept = reinterpret_cast<AcceptFn>(get("accept"));
    c.recv = reinterpret_cast<RecvFn>(get("recv"));
    c.send = reinterpret_cast<SendFn>(get("send"));
    c.closesocket = reinterpret_cast<CloseFn>(get("closesocket"));
    c.select = reinterpret_cast<SelectFn>(get("select"));
    c.setsockopt = reinterpret_cast<SetOptFn>(get("setsockopt"));
    c.startup = reinterpret_cast<StartupFn>(get("WSAStartup"));
    c.connect = reinterpret_cast<ConnectFn>(get("connect"));
    return c;
}

std::atomic<bool> g_stopping{false};
HANDLE g_thread = nullptr;
HWND g_window = nullptr;
int g_port = 0;
std::wstring g_dir;  // the client folder
std::wstring g_ini;  // uaro-graphics.ini (the app's Paths::ini)
Calls g_calls;

// A call for the window's thread; `state` 0 waiting, 1 answered, 2 given up by the server (the window frees it)
struct Job {
    std::string tool;
    json::Value args;
    mcp::CallResult result;
    std::atomic<int> state{0};
};

mcp::CallResult said(const std::string& text, bool error = false) {
    mcp::CallResult r;
    r.isError = error;
    r.content.push_back(mcp::Content::textOf(text));
    return r;
}

std::string lowered(std::wstring w) {
    std::transform(w.begin(), w.end(), w.begin(), [](wchar_t c) { return wchar_t(towlower(c)); });
    return utf8(w);
}

// The processes of `exe` (a file's name) whose path lies in the client folder
std::vector<DWORD> processesOf(const wchar_t* exe) {
    std::vector<DWORD> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W e{};
    e.dwSize = sizeof e;
    const std::string dir = lowered(g_dir) + "\\";
    for (BOOL more = Process32FirstW(snap, &e); more; more = Process32NextW(snap, &e)) {
        if (lstrcmpiW(e.szExeFile, exe) != 0) continue;
        HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
        if (!p) continue;
        wchar_t path[MAX_PATH];
        DWORD n = MAX_PATH;
        if (QueryFullProcessImageNameW(p, 0, path, &n) && lowered(std::wstring(path, n)).rfind(dir, 0) == 0)
            out.push_back(e.th32ProcessID);
        CloseHandle(p);
    }
    CloseHandle(snap);
    return out;
}

struct WindowsOf {
    DWORD pid = 0;
    std::vector<HWND> found;
};
BOOL CALLBACK collect(HWND w, LPARAM lp) {
    auto* x = reinterpret_cast<WindowsOf*>(lp);
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid == x->pid && IsWindowVisible(w) && !GetWindow(w, GW_OWNER)) x->found.push_back(w);
    return TRUE;
}
std::vector<HWND> windowsOf(DWORD pid) {
    WindowsOf x;
    x.pid = pid;
    EnumWindows(&collect, reinterpret_cast<LPARAM>(&x));
    return x.found;
}

bool hung(HWND w) {
    using Fn = BOOL(WINAPI*)(HWND);
    static const Fn f =
        reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"),
                                                                    "IsHungAppWindow")));
    return f && f(w);
}

double megabytes(DWORD pid) {
    struct Counters {
        DWORD cb;
        DWORD pageFaults;
        SIZE_T peakWorkingSet, workingSet, quotaPeakPaged, quotaPaged, quotaPeakNonPaged, quotaNonPaged, pagefile,
            peakPagefile;
    };
    using Fn = BOOL(WINAPI*)(HANDLE, Counters*, DWORD);
    static const Fn get = reinterpret_cast<Fn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "K32GetProcessMemoryInfo")));
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return -1;
    Counters c{};
    c.cb = sizeof c;
    const bool ok = get && get(p, &c, sizeof c);
    CloseHandle(p);
    return ok ? double(c.workingSet) / (1024.0 * 1024.0) : -1;
}

uint64_t ticks(const FILETIME& f) { return (uint64_t(f.dwHighDateTime) << 32) | f.dwLowDateTime; }

uint64_t processBusy(DWORD pid, uint64_t* created = nullptr) {
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return 0;
    FILETIME c{}, e{}, k{}, u{};
    const bool ok = GetProcessTimes(p, &c, &e, &k, &u);
    CloseHandle(p);
    if (created && ok) *created = ticks(c);
    return ok ? ticks(k) + ticks(u) : 0;
}

bool gameMcpAnswers() {
    const SOCKET s = g_calls.socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    const int port = g_port - 1;
    a.sin_port = uint16_t((port >> 8 & 0xFF) | (port & 0xFF) << 8);
    a.sin_addr.s_addr = 0x0100007Fu;
    const bool ok = g_calls.connect(s, reinterpret_cast<const sockaddr*>(&a), sizeof a) == 0;
    g_calls.closesocket(s);
    return ok;
}

mcp::CallResult gameTool(const json::Value& args) {
    const std::string action = args["action"].asString();
    if (action == "status") {
        json::Value out = json::Value::object();
        json::Value games = json::Value::array();
        FILETIME nowF{};
        GetSystemTimeAsFileTime(&nowF);
        for (const DWORD pid : processesOf(L"uaRO.exe")) {
            json::Value g = json::Value::object();
            g.set("pid", int64_t(pid));
            uint64_t created = 0;
            processBusy(pid, &created);
            if (created) g.set("upSeconds", int64_t((ticks(nowF) - created) / 10000000ull));
            const std::vector<HWND> ws = windowsOf(pid);
            g.set("windows", int(ws.size()));
            bool anyHung = false;
            for (HWND w : ws) anyHung = anyHung || hung(w);
            g.set("hung", anyHung);
            const double mb = megabytes(pid);
            if (mb >= 0) g.set("memoryMb", int64_t(mb));
            games.push(g);
        }
        out.set("games", games);
        out.set("renderers", int(processesOf(L"uaro-render64.exe").size()));
        out.set("gameMcpAnswers", gameMcpAnswers());
        return said(json::dump(out));
    }
    if (action == "kill") {
        int ended = 0;
        for (const wchar_t* exe : {L"uaRO.exe", L"uaro-render64.exe"})
            for (const DWORD pid : processesOf(exe)) {
                HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                if (p && TerminateProcess(p, 1)) ++ended;
                if (p) CloseHandle(p);
            }
        return said("ended " + std::to_string(ended) + " process(es)");
    }
    // stop: each game window asked to close as its own close does; up to 15 s for them to go
    const std::vector<DWORD> games = processesOf(L"uaRO.exe");
    if (games.empty()) return said("the game is not running");
    for (const DWORD pid : games)
        for (HWND w : windowsOf(pid)) PostMessageW(w, WM_CLOSE, 0, 0);
    const ULONGLONG start = GetTickCount64();
    while (GetTickCount64() - start < 15000 && !processesOf(L"uaRO.exe").empty() && !g_stopping) Sleep(250);
    const size_t left = processesOf(L"uaRO.exe").size();
    return left ? said(std::to_string(left) + " game process(es) still running after 15 s - kill them?", true)
                : said("the game closed");
}

// The computer's load over `seconds`: the processor (all; the game's, the renderer's), memory, the power, an NVIDIA
// card
mcp::CallResult systemTool(const json::Value& args) {
    const int seconds = args.has("seconds") ? int(args["seconds"].asInt()) : 2;
    using TimesFn = BOOL(WINAPI*)(FILETIME*, FILETIME*, FILETIME*);
    static const TimesFn systemTimes = reinterpret_cast<TimesFn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemTimes")));
    const std::vector<DWORD> games = processesOf(L"uaRO.exe"), renderers = processesOf(L"uaro-render64.exe");
    const auto busy = [](const std::vector<DWORD>& ps) {
        uint64_t t = 0;
        for (const DWORD p : ps) t += processBusy(p);
        return t;
    };
    FILETIME i0{}, k0{}, u0{}, i1{}, k1{}, u1{};
    const bool sys0 = systemTimes && systemTimes(&i0, &k0, &u0);
    const uint64_t g0 = busy(games), r0 = busy(renderers);
    const ULONGLONG wall0 = GetTickCount64();
    Sleep(DWORD(seconds) * 1000);
    const bool sys1 = systemTimes && systemTimes(&i1, &k1, &u1);
    const uint64_t g1 = busy(games), r1 = busy(renderers);
    const double wall = double(GetTickCount64() - wall0) * 10000.0;  // in 100 ns
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const double cpus = double(std::max<DWORD>(1, si.dwNumberOfProcessors));
    json::Value out = json::Value::object();
    json::Value cpu = json::Value::object();
    if (sys0 && sys1) {
        const double total = double(ticks(k1) - ticks(k0) + ticks(u1) - ticks(u0));
        const double idle = double(ticks(i1) - ticks(i0));
        if (total > 0) cpu.set("allPercent", int64_t((total - idle) * 100.0 / total + 0.5));
    }
    if (wall > 0) {
        cpu.set("gamePercent", int64_t(double(g1 - g0) * 100.0 / (wall * cpus) + 0.5));
        cpu.set("rendererPercent", int64_t(double(r1 - r0) * 100.0 / (wall * cpus) + 0.5));
        cpu.set("gameOfOneCore", int64_t(double(g1 - g0) * 100.0 / wall + 0.5));
    }
    cpu.set("threads", int64_t(cpus));
    out.set("cpu", cpu);
    MEMORYSTATUSEX m{};
    m.dwLength = sizeof m;
    json::Value mem = json::Value::object();
    if (GlobalMemoryStatusEx(&m)) {
        mem.set("totalMb", int64_t(m.ullTotalPhys / (1024 * 1024)));
        mem.set("freeMb", int64_t(m.ullAvailPhys / (1024 * 1024)));
    }
    double gm = 0, rm = 0;
    for (const DWORD p : games) gm += std::max(0.0, megabytes(p));
    for (const DWORD p : renderers) rm += std::max(0.0, megabytes(p));
    mem.set("gameMb", int64_t(gm));
    mem.set("rendererMb", int64_t(rm));
    out.set("memory", mem);
    SYSTEM_POWER_STATUS ps{};
    if (GetSystemPowerStatus(&ps) && ps.ACLineStatus <= 1) {
        json::Value p = json::Value::object();
        p.set("mains", ps.ACLineStatus == 1);
        if (ps.BatteryLifePercent <= 100) p.set("batteryPercent", int(ps.BatteryLifePercent));
        out.set("power", p);
    }
    // an NVIDIA card: nvidia-smi's one line, its output through a file (no pipes imported)
    wchar_t sys[MAX_PATH] = {};
    GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring smi = std::wstring(sys) + L"\\nvidia-smi.exe";
    if (GetFileAttributesW(smi.c_str()) == INVALID_FILE_ATTRIBUTES)
        smi = L"C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvidia-smi.exe";
    if (GetFileAttributesW(smi.c_str()) != INVALID_FILE_ATTRIBUTES) {
        wchar_t tmp[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, tmp);
        const std::wstring outFile = std::wstring(tmp) + L"uaro-mcp-gpu.txt";
        SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
        HANDLE f = CreateFileW(outFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            STARTUPINFOW st{};
            st.cb = sizeof st;
            st.dwFlags = STARTF_USESTDHANDLES;
            st.hStdOutput = f;
            st.hStdError = f;
            PROCESS_INFORMATION pi{};
            std::wstring line = L"\"" + smi + L"\" " + wide(::uagfx::mcpwatch::kNvidiaQuery);
            if (CreateProcessW(smi.c_str(), line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
                               &st, &pi)) {
                WaitForSingleObject(pi.hProcess, 5000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
            CloseHandle(f);
            std::string text;
            HANDLE r = CreateFileW(outFile.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                   OPEN_EXISTING, 0, nullptr);
            if (r != INVALID_HANDLE_VALUE) {
                char buf[4096];
                DWORD got = 0;
                if (ReadFile(r, buf, sizeof buf, &got, nullptr)) text.assign(buf, got);
                CloseHandle(r);
            }
            DeleteFileW(outFile.c_str());
            const json::Value g = ::uagfx::mcpwatch::nvidiaSmi(text);
            if (g.type() != json::Value::Null) out.set("nvidia", g);
        }
    }
    return said(json::dump(out));
}

std::wstring ourDir() { return g_ini.substr(0, g_ini.find_last_of(L'\\')); }

std::string readTail(const std::wstring& path, size_t most) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size{};
    GetFileSizeEx(f, &size);
    const LONGLONG want = std::min<LONGLONG>(size.QuadPart, LONGLONG(most));
    LARGE_INTEGER at{};
    at.QuadPart = size.QuadPart - want;
    SetFilePointerEx(f, at, nullptr, FILE_BEGIN);
    std::string text(size_t(want), '\0');
    DWORD got = 0;
    if (want > 0) ReadFile(f, &text[0], DWORD(want), &got, nullptr);
    CloseHandle(f);
    text.resize(got);
    return text;
}

mcp::CallResult filesTool(const json::Value& args) {
    const std::string action = args["action"].asString();
    if (action == "list") {
        json::Value list = json::Value::array();
        for (const std::wstring& sub : {std::wstring(), std::wstring(L"logs\\")}) {
            WIN32_FIND_DATAW d{};
            HANDLE h = FindFirstFileW((ourDir() + L"\\" + sub + L"*").c_str(), &d);
            if (h == INVALID_HANDLE_VALUE) continue;
            do {
                if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                const std::string name = utf8(sub + d.cFileName);
                if (!::uagfx::mcpwatch::fileAllowed(name)) continue;
                json::Value x = json::Value::object();
                x.set("name", name);
                x.set("bytes", int64_t((uint64_t(d.nFileSizeHigh) << 32) | d.nFileSizeLow));
                FILETIME local{};
                SYSTEMTIME t{};
                FileTimeToLocalFileTime(&d.ftLastWriteTime, &local);
                FileTimeToSystemTime(&local, &t);
                char when[32];
                std::snprintf(when, sizeof when, "%04u-%02u-%02u %02u:%02u:%02u", t.wYear, t.wMonth, t.wDay, t.wHour,
                              t.wMinute, t.wSecond);
                x.set("modified", std::string(when));
                list.push(x);
            } while (FindNextFileW(h, &d));
            FindClose(h);
        }
        return said(json::dump(list));
    }
    std::wstring path = ourDir() + L"\\" + wide(args["name"].asString());
    std::replace(path.begin(), path.end(), L'/', L'\\');
    if (action == "tail") {
        const std::string text = readTail(path, size_t(1) << 20);
        const std::string out =
            mcp::tailLines(text, size_t(args["lines"].asInt(100)), "", args["since"].asString());
        return said(out.empty() ? "(no such lines)" : out);
    }
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a)) return said("no such file", true);
    const uint64_t size = (uint64_t(a.nFileSizeHigh) << 32) | a.nFileSizeLow;
    if (size > (8u << 20)) return said("the file is over 8 MB", true);
    json::Value out = json::Value::object();
    out.set("name", args["name"].asString());
    out.set("bytes", int64_t(size));
    out.set("base64", report::base64(readTail(path, size_t(size))));
    return said(json::dump(out));
}

mcp::CallResult iniGet(const json::Value& args) {
    const std::string key = args["key"].asString();
    const settings::IniSetting* s = settings::findSetting(key);
    if (!s) return said("no such setting", true);
    const Config c = parseConfig(readTail(g_ini, size_t(4) << 20));
    json::Value out = json::Value::object();
    out.set("key", key);
    out.set("value", settings::tokenOf(*s, c));
    return said(json::dump(out));
}

// uaro-graphics.ini's key set (checked by core/mcp_watch as Alt+O would): its line changed in place, the rest as it was
mcp::CallResult iniSet(const json::Value& args) {
    const std::string key = args["key"].asString(), value = args["value"].asString();
    const size_t dot = key.find('.');
    const std::wstring& path = g_ini;
    const std::string text = settings::applyIniEdits(readTail(path, size_t(4) << 20),
                                                     {{key.substr(0, dot), key.substr(dot + 1), value}});
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return said("uaro-graphics.ini could not be written", true);
    DWORD put = 0;
    const bool ok = WriteFile(f, text.data(), DWORD(text.size()), &put, nullptr) && put == text.size();
    CloseHandle(f);
    return ok ? said("written: " + key + " = " + value + " (the game takes most at once, some at its next start)")
              : said("uaro-graphics.ini could not be written whole", true);
}

// The window's thread's calls: posted, waited for (20 s at most)
mcp::CallResult onWindow(const std::string& tool, const json::Value& args) {
    auto* job = new Job;
    job->tool = tool;
    job->args = args;
    if (!PostMessageW(g_window, kCall, 0, reinterpret_cast<LPARAM>(job))) {
        delete job;
        return said("the app's window did not take the call", true);
    }
    const ULONGLONG start = GetTickCount64();
    while (job->state.load() == 0 && GetTickCount64() - start < 20000 && !g_stopping) Sleep(20);
    int waiting = 0;
    if (job->state.compare_exchange_strong(waiting, 2)) return said("the app's window did not answer in 20 s", true);
    mcp::CallResult r = job->result;
    delete job;
    return r;
}

mcp::CallResult onCall(const std::string& tool, const json::Value& args) {
    const std::string action = args["action"].asString();
    if (tool == "game" && action == "start") return onWindow(tool, args);  // the model's launch
    if (tool == "game") return gameTool(args);
    if (tool == "system") return systemTool(args);
    if (tool == "files") return filesTool(args);
    if (tool == "ini") return action == "set" ? iniSet(args) : iniGet(args);
    return said("no such tool: " + tool, true);
}

void sendAll(SOCKET s, const std::string& data) {
    size_t at = 0;
    while (at < data.size()) {
        const int n = g_calls.send(s, data.data() + at, int(std::min<size_t>(data.size() - at, 1 << 16)), 0);
        if (n <= 0) return;
        at += size_t(n);
    }
}

void serveOne(SOCKET s) {
    std::string data;
    mcp::HttpRequest req;
    int got = 0;
    const ULONGLONG start = GetTickCount64();
    char buf[16384];
    while ((got = mcp::parseHttp(data, req)) == 0 && GetTickCount64() - start < 5000 && !g_stopping) {
        fd_set r{};
        r.fd_count = 1;
        r.fd_array[0] = s;
        const timeval wait{0, 250000};
        if (g_calls.select(0, &r, nullptr, nullptr, &wait) <= 0 || r.fd_count == 0) continue;
        const int n = g_calls.recv(s, buf, sizeof buf, 0);
        if (n <= 0) return;
        data.append(buf, size_t(n));
    }
    std::string answer, why;
    if (got <= 0) {
        answer = mcp::httpResponse(got < 0 ? 413 : 400, "text/plain", "a bad or too long request\n");
    } else if (!mcp::allowed(req, g_port, why)) {
        answer = mcp::httpResponse(403, "text/plain", why + "\n");
    } else if (req.path == "/mcp" || req.path.rfind("/mcp?", 0) == 0) {
        if (req.method != "POST") {
            answer = mcp::httpResponse(405, "text/plain", "POST /mcp - one JSON-RPC message a request\n");
        } else {
            const std::string out = mcp::handle(req.body, &onCall, kVersion, ::uagfx::mcpwatch::catalog());
            answer = out.empty() ? mcp::httpResponse(202, "", "") : mcp::httpResponse(200, "application/json", out);
        }
    } else if (req.method == "GET" && req.path == "/") {
        answer = mcp::httpResponse(200, "text/plain",
                                   std::string("uaRO Graphics watchdog ") + kVersion + " - MCP at POST /mcp\n");
    } else {
        answer = mcp::httpResponse(404, "text/plain", "MCP at POST /mcp\n");
    }
    sendAll(s, answer);
}

DWORD WINAPI serve(void*) {
    WSADATA wsa{};
    g_calls.startup(MAKEWORD(2, 2), &wsa);
    const SOCKET l = g_calls.socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (l == INVALID_SOCKET) return 1;
    const BOOL exclusive = TRUE;
    g_calls.setsockopt(l, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof exclusive);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = uint16_t((g_port >> 8 & 0xFF) | (g_port & 0xFF) << 8);  // network order
    a.sin_addr.s_addr = 0x0100007Fu;                                      // 127.0.0.1, network order
    if (g_calls.bind(l, reinterpret_cast<const sockaddr*>(&a), sizeof a) != 0 || g_calls.listen(l, 4) != 0) {
        g_calls.closesocket(l);
        return 1;
    }
    while (!g_stopping) {
        fd_set r{};
        r.fd_count = 1;
        r.fd_array[0] = l;
        const timeval wait{0, 250000};
        if (g_calls.select(0, &r, nullptr, nullptr, &wait) <= 0 || r.fd_count == 0) continue;
        const SOCKET s = g_calls.accept(l, nullptr, nullptr);
        if (s == INVALID_SOCKET) continue;
        serveOne(s);
        g_calls.closesocket(s);
    }
    g_calls.closesocket(l);
    return 0;
}

}  // namespace

void start(HWND window, int port, const std::wstring& clientDir, const std::wstring& iniPath) {
    if (g_thread || port <= 0 || port > 65535 || iniPath.find(L'\\') == std::wstring::npos) return;
    g_ini = iniPath;
    g_calls = calls();
    if (!g_calls.ok()) return;
    g_window = window;
    g_port = port;
    g_dir = clientDir;
    g_stopping = false;
    g_thread = CreateThread(nullptr, 0, &serve, nullptr, 0, nullptr);
}

void stop() {
    if (!g_thread) return;
    g_stopping = true;
    WaitForSingleObject(g_thread, 3000);
    CloseHandle(g_thread);
    g_thread = nullptr;
}

void run(LPARAM p, Model& model, settings::Lang lang) {
    auto* job = reinterpret_cast<Job*>(p);
    if (!job) return;
    // the game's start (the one call that needs the model): as the launcher's play does
    if (!processesOf(L"uaRO.exe").empty() && !job->args["another"].asBool()) {
        job->result = said("the game runs already (another: true starts one more window)", true);
    } else {
        const std::wstring err = model.launchGame(lang);
        job->result = err.empty() ? said("the game started") : said(utf8(err), true);
    }
    int waiting = 0;
    if (!job->state.compare_exchange_strong(waiting, 1)) delete job;  // the server gave up on it: ours to free
}

}  // namespace uagfx::app::mcpwatch
