#include "win/mcp_server.hpp"

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/version.hpp"
#include "win/log.hpp"
#include "win/mcp_input.hpp"

namespace uagfx::win::mcpwin {
namespace {

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
using ErrorFn = int(WSAAPI*)();

// The game's own ws2_32 (it imports it; the driver must not)
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
    ErrorFn lastError = nullptr;
    bool ok() const {
        return socket && bind && listen && accept && recv && send && closesocket && select && setsockopt && startup &&
               lastError;
    }
};

Calls calls() {
    Calls c;
    const HMODULE ws = GetModuleHandleW(L"ws2_32.dll");
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
    c.lastError = reinterpret_cast<ErrorFn>(get("WSAGetLastError"));
    return c;
}

// The call the server thread waits on; one at a time (one connection at a time). `seq` tells a late answer from a
// newer call's
enum class State : int { None, Waiting, ShotAsked, ShotSent, Done };
struct Job {
    uint64_t seq = 0;
    std::string tool;
    std::string args;  // its arguments, JSON
    State state = State::None;
    mcp::CallResult result;
    int width = 1280;  // a screenshot's
    std::string shotFile;
    ULONGLONG at = 0;
};

constexpr DWORD kAnswerMs = 10000, kShotAnswerMs = 15000;
constexpr ULONGLONG kShotWaitMs = 12000, kRetryMs = 10000;
constexpr const char* kShotName = "\\uaro-graphics\\mcp-shot.png";

SRWLOCK g_lock = SRWLOCK_INIT;  // the job (MinGW here has no std::mutex)
Job g_job;
uint64_t g_seq = 0;
HANDLE g_done = nullptr;  // set when the job is answered
HANDLE g_thread = nullptr;
std::atomic<bool> g_stopping{false};
int g_port = 0;
std::string g_gameDir;
ULONGLONG g_failedAt = 0;
Seen g_seen;  // 3.0.22-beta.35: the last frame's

struct Locked {
    Locked() { AcquireSRWLockExclusive(&g_lock); }
    ~Locked() { ReleaseSRWLockExclusive(&g_lock); }
};

bool wholePng(const std::string& file) {
    FILE* f = std::fopen(file.c_str(), "rb");
    if (!f) return false;
    unsigned char head[8] = {}, tail[12] = {};
    bool ok = std::fread(head, 1, 8, f) == 8 && std::memcmp(head, "\x89PNG", 4) == 0 &&
              std::fseek(f, -12, SEEK_END) == 0 && std::fread(tail, 1, 12, f) == 12 &&
              std::memcmp(tail + 4, "IEND", 4) == 0;
    std::fclose(f);
    return ok;
}

std::string fileBytes(const std::string& file) {
    std::string out;
    FILE* f = std::fopen(file.c_str(), "rb");
    if (!f) return out;
    char buf[65536];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return out;
}

mcp::CallResult failure(const std::string& text) {
    mcp::CallResult r;
    r.isError = true;
    r.content.push_back(mcp::Content::textOf(text));
    return r;
}

// The server thread's side of a tool: the game's thread asked, its answer waited for
mcp::CallResult onGame(const std::string& tool, const json::Value& args) {
    if (tool == "key" || tool == "click") {  // 3.0.22-beta.35: real input, from here - the game's frames go on
        Seen seen;
        {
            Locked l;
            seen = g_seen;
        }
        const mcpinput::Game g{seen.window, seen.gameWidth, seen.gameHeight};
        if (tool == "click")
            return mcpinput::click(g, args["x"].asNumber(), args["y"].asNumber(), int(args["width"].asInt()),
                                   args["button"].asString() == "right", args["double"].asBool(false));
        mcp::KeyCombo k;
        std::string why;
        mcp::keyCombo(args["keys"].asString(), k, why);  // checked already
        return mcpinput::key(g, k, args["keys"].asString());
    }
    uint64_t seq = 0;
    {
        Locked l;
        g_job = Job{};
        g_job.seq = seq = ++g_seq;
        g_job.tool = tool;
        g_job.args = json::dump(args);
        g_job.state = State::Waiting;
        g_job.width = args.has("width") ? int(args["width"].asInt(1280)) : 1280;
        g_job.at = GetTickCount64();
        ResetEvent(g_done);
    }
    const bool shot = tool == "screenshot";
    const bool answered = WaitForSingleObject(g_done, shot ? kShotAnswerMs : kAnswerMs) == WAIT_OBJECT_0;
    Job j;
    {
        Locked l;
        if (g_job.seq == seq) j = g_job;
        g_job = Job{};
    }
    if (!answered || j.state != State::Done)
        return failure("the game did not answer in time - a map loading? (10 s; a screenshot 15 s)");
    if (!shot || j.result.isError) return j.result;
    const std::string png = fileBytes(j.shotFile);
    if (png.empty()) return failure("the screenshot could not be read");
    mcp::CallResult r;
    r.content.push_back(mcp::Content::imageOf(png, "image/png"));
    r.content.push_back(mcp::Content::textOf("PNG, " + std::to_string(png.size() / 1024) + " KB"));
    return r;
}

void sendAll(const Calls& c, SOCKET s, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        const int n = c.send(s, data.data() + sent, int(std::min<size_t>(data.size() - sent, 65536)), 0);
        if (n <= 0) return;
        sent += size_t(n);
    }
}

void serveOne(const Calls& c, SOCKET s, int port) {
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
        if (c.select(0, &r, nullptr, nullptr, &wait) <= 0 || r.fd_count == 0) continue;
        const int n = c.recv(s, buf, sizeof buf, 0);
        if (n <= 0) return;
        data.append(buf, size_t(n));
    }
    std::string answer;
    std::string why;
    static int refusedTold = 0;
    if (got <= 0) {
        answer = mcp::httpResponse(got < 0 ? 413 : 400, "text/plain", "a bad or too long request\n");
    } else if (!mcp::allowed(req, port, why)) {
        if (refusedTold++ < 10) LOG_INFO("mcp: a request refused - %s", why.c_str());
        answer = mcp::httpResponse(403, "text/plain", why + "\n");
    } else if (req.path == "/mcp" || req.path.rfind("/mcp?", 0) == 0) {
        if (req.method != "POST") {
            answer = mcp::httpResponse(405, "text/plain", "POST /mcp - one JSON-RPC message a request\n");
        } else {
            const std::string out = mcp::handle(req.body, &onGame, kVersion);
            answer = out.empty() ? mcp::httpResponse(202, "", "") : mcp::httpResponse(200, "application/json", out);
        }
    } else if (req.method == "GET" && req.path == "/") {
        answer =
            mcp::httpResponse(200, "text/plain", std::string("uaRO Graphics ") + kVersion + " - MCP at POST /mcp\n");
    } else {
        answer = mcp::httpResponse(404, "text/plain", "MCP at POST /mcp\n");
    }
    sendAll(c, s, answer);
}

DWORD WINAPI serve(void* p) {
    const int port = int(reinterpret_cast<intptr_t>(p));
    const Calls c = calls();
    if (!c.ok()) {
        LOG_INFO("mcp: the game's Winsock not there - not listening");
        return 1;
    }
    WSADATA wsa{};
    c.startup(MAKEWORD(2, 2), &wsa);
    const SOCKET l = c.socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (l == INVALID_SOCKET) {
        LOG_INFO("mcp: no socket (error %d) - not listening", c.lastError());
        return 1;
    }
    const BOOL exclusive = TRUE;
    c.setsockopt(l, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof exclusive);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = uint16_t((port >> 8 & 0xFF) | (port & 0xFF) << 8);  // network order
    a.sin_addr.s_addr = 0x0100007Fu;                                  // 127.0.0.1, network order
    if (c.bind(l, reinterpret_cast<const sockaddr*>(&a), sizeof a) != 0 || c.listen(l, 4) != 0) {
        LOG_INFO("mcp: 127.0.0.1:%d not taken (error %d) - another program there? [mcp] port", port, c.lastError());
        c.closesocket(l);
        return 1;
    }
    LOG_INFO("mcp: listening on 127.0.0.1:%d - POST /mcp ([mcp] enabled)", port);
    while (!g_stopping) {
        fd_set r{};
        r.fd_count = 1;
        r.fd_array[0] = l;
        const timeval wait{0, 250000};
        if (c.select(0, &r, nullptr, nullptr, &wait) <= 0 || r.fd_count == 0) continue;
        const SOCKET s = c.accept(l, nullptr, nullptr);
        if (s == INVALID_SOCKET) continue;
        serveOne(c, s, port);
        c.closesocket(s);
    }
    c.closesocket(l);
    LOG_INFO("mcp: 127.0.0.1:%d closed", port);
    return 0;
}

void stopThread() {
    if (!g_thread) return;
    {
        Locked l;  // a call waiting: answered now, so the thread can end
        if (g_job.state != State::None && g_job.state != State::Done) {
            g_job.result = failure("the MCP server was turned off");
            g_job.state = State::Done;
            SetEvent(g_done);
        }
    }
    g_stopping = true;
    WaitForSingleObject(g_thread, 3000);
    CloseHandle(g_thread);
    g_thread = nullptr;
    g_stopping = false;
}

}  // namespace

void frame(bool enabled, int port, const std::string& gameDir, const Seen& seen, const RunFn& run) {
    if (!g_done) g_done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_done) return;
    const bool want = enabled && port >= 1024 && port <= 65535;
    if (g_thread && WaitForSingleObject(g_thread, 0) == WAIT_OBJECT_0) {  // ended by itself: the port not taken
        DWORD code = 0;
        GetExitCodeThread(g_thread, &code);
        CloseHandle(g_thread);
        g_thread = nullptr;
        if (code != 0) g_failedAt = GetTickCount64();
    }
    if (g_thread && (!want || port != g_port)) stopThread();
    if (want && !g_thread && (g_failedAt == 0 || GetTickCount64() - g_failedAt >= kRetryMs || port != g_port)) {
        {
            Locked l;
            g_gameDir = gameDir;
        }
        g_port = port;
        g_failedAt = 0;
        g_thread = CreateThread(nullptr, 0, &serve, reinterpret_cast<void*>(intptr_t(port)), 0, nullptr);
    }
    // the call waiting
    std::string tool, args;
    uint64_t seq = 0;
    {
        Locked l;
        Job& j = g_job;
        g_seen = seen;
        const bool shotWaits = j.tool == "screenshot" && (j.state == State::Waiting || j.state == State::ShotAsked);
        if (shotWaits && !seen.shown) {  // 3.0.22-beta.35: minimised - no picture made, said at once
            j.result = failure("the game's window is minimised - no picture to take (the other tools work)");
            j.state = State::Done;
            SetEvent(g_done);
        } else if (j.state == State::Waiting && j.tool == "screenshot") {
            j.state = State::ShotAsked;  // shotWanted puts it into this frame's present
        } else if (j.state == State::Waiting) {
            tool = j.tool;
            args = j.args;
            seq = j.seq;
            j.state = State::None;  // taken: run below, out of the lock
        } else if (j.state == State::ShotSent) {
            if (wholePng(j.shotFile)) {
                j.state = State::Done;
                SetEvent(g_done);
            } else if (GetTickCount64() - j.at > kShotWaitMs) {
                j.result = failure("the screenshot was not written - the game's window must be shown");
                j.state = State::Done;
                SetEvent(g_done);
            }
        }
    }
    if (tool.empty()) return;
    json::Value a;
    std::string error;
    json::parse(args, a, error);
    LOG_INFO("mcp: %s %s", tool.c_str(), args.size() > 300 ? (args.substr(0, 300) + "...").c_str() : args.c_str());
    mcp::CallResult r = run(tool, a);
    Locked l;
    if (g_job.seq != seq) return;  // waited too long: nobody asks any more
    g_job.result = std::move(r);
    g_job.state = State::Done;
    SetEvent(g_done);
}

std::string shotWanted(int pictureWidth) {
    Locked l;
    if (g_job.state != State::ShotAsked || g_gameDir.empty()) return "";
    const std::string file = g_gameDir + kShotName;
    DeleteFileA(file.c_str());
    const int want = std::clamp(g_job.width, 320, 1920);
    const int n = std::clamp(pictureWidth > 0 ? int(std::ceil(double(pictureWidth) / double(want))) : 1, 1, 8);
    g_job.shotFile = file;
    g_job.state = State::ShotSent;
    g_job.at = GetTickCount64();
    return n > 1 ? file + "|/" + std::to_string(n) : file;
}

void stop() { stopThread(); }

}  // namespace uagfx::win::mcpwin
