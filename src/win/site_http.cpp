#include "win/site_http.hpp"

#include <windows.h>
#include <winhttp.h>

#include <deque>
#include <memory>
#include <vector>

#include "win/code_page.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/site_key.hpp"

namespace uagfx::win::sitehttp {
namespace {

constexpr const wchar_t* kHost = L"uaro.kiev.ua";
constexpr int kTimeoutMs = 15000;

// winhttp.dll's functions, asked for at the first request
struct Http {
    HINTERNET(WINAPI* open)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD) = nullptr;
    BOOL(WINAPI* timeouts)(HINTERNET, int, int, int, int) = nullptr;
    HINTERNET(WINAPI* connect)(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD) = nullptr;
    HINTERNET(WINAPI* request)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD) = nullptr;
    BOOL(WINAPI* send)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR) = nullptr;
    BOOL(WINAPI* receive)(HINTERNET, LPVOID) = nullptr;
    BOOL(WINAPI* query)(HINTERNET, DWORD, LPCWSTR, LPVOID, LPDWORD, LPDWORD) = nullptr;
    BOOL(WINAPI* available)(HINTERNET, LPDWORD) = nullptr;
    BOOL(WINAPI* read)(HINTERNET, LPVOID, DWORD, LPDWORD) = nullptr;
    BOOL(WINAPI* close)(HINTERNET) = nullptr;
    bool ok = false;
};

template <typename F>
void take(HMODULE m, F& f, const char* name) {
    f = reinterpret_cast<F>(reinterpret_cast<void*>(GetProcAddress(m, name)));
}

const Http& http() {
    static const Http h = [] {
        Http x;
        const HMODULE m = LoadLibraryA("winhttp.dll");
        if (!m) return x;
        take(m, x.open, "WinHttpOpen");
        take(m, x.timeouts, "WinHttpSetTimeouts");
        take(m, x.connect, "WinHttpConnect");
        take(m, x.request, "WinHttpOpenRequest");
        take(m, x.send, "WinHttpSendRequest");
        take(m, x.receive, "WinHttpReceiveResponse");
        take(m, x.query, "WinHttpQueryHeaders");
        take(m, x.available, "WinHttpQueryDataAvailable");
        take(m, x.read, "WinHttpReadData");
        take(m, x.close, "WinHttpCloseHandle");
        x.ok = x.open && x.timeouts && x.connect && x.request && x.send && x.receive && x.query && x.available &&
               x.read && x.close;
        return x;
    }();
    return h;
}

struct Job {
    siteapi::Request request;
    std::string token;  // the device token (read on the game's thread)
    Answer answer;
    int status = 0;
    std::string body;
};

struct Done {
    SRWLOCK lock = SRWLOCK_INIT;
    std::deque<std::unique_ptr<Job>> jobs;
    LONG inFlight = 0;
};

Done& done() {
    static Done d;
    return d;
}

// One request: its status (0 not reached) and its body (1 MB at most)
void run(Job& j) {
    const Http& h = http();
    if (!h.ok) return;
    HINTERNET session = h.open(L"uaRO Graphics", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return;
    h.timeouts(session, kTimeoutMs, kTimeoutMs, kTimeoutMs, kTimeoutMs);
    HINTERNET connect = h.connect(session, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    const std::wstring method = utf8Wide(j.request.method), path = utf8Wide(j.request.path);
    HINTERNET request = connect ? h.request(connect, method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                                : nullptr;
    std::wstring headers = L"Accept: application/json\r\n";
    if (!j.request.body.empty()) headers += L"Content-Type: application/json\r\n";
    if (j.request.auth && !j.token.empty()) headers += L"Authorization: Bearer " + utf8Wide(j.token) + L"\r\n";
    DWORD status = 0, size = sizeof status;
    const bool sent =
        request &&
        h.send(request, headers.c_str(), DWORD(-1L), j.request.body.empty() ? WINHTTP_NO_REQUEST_DATA
                                                                            : const_cast<char*>(j.request.body.data()),
               DWORD(j.request.body.size()), DWORD(j.request.body.size()), 0) &&
        h.receive(request, nullptr) &&
        h.query(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                &status, &size, WINHTTP_NO_HEADER_INDEX);
    if (sent) {
        j.status = int(status);
        DWORD n = 0;
        std::vector<char> part;
        while (h.available(request, &n) && n && j.body.size() < (1u << 20)) {
            part.resize(n);
            DWORD got = 0;
            if (!h.read(request, part.data(), n, &got) || !got) break;
            j.body.append(part.data(), got);
        }
    }
    if (!headers.empty()) SecureZeroMemory(headers.data(), headers.size() * sizeof(wchar_t));
    if (request) h.close(request);
    if (connect) h.close(connect);
    h.close(session);
}

DWORD WINAPI worker(LPVOID p) {
    std::unique_ptr<Job> j(static_cast<Job*>(p));
    run(*j);
    Done& d = done();
    AcquireSRWLockExclusive(&d.lock);
    d.jobs.push_back(std::move(j));
    ReleaseSRWLockExclusive(&d.lock);
    return 0;
}

std::string pathOnly(const std::string& path) { return path.substr(0, path.find('?')); }

}  // namespace

void send(const siteapi::Request& r, Answer answer, const std::string& token) {
    auto j = std::make_unique<Job>();
    j->request = r;
    if (r.auth) j->token = token.empty() ? sitekey::token() : token;
    j->answer = std::move(answer);
    InterlockedIncrement(&done().inFlight);
    Job* raw = j.release();
    const HANDLE t = CreateThread(nullptr, 0, worker, raw, 0, nullptr);
    if (t) {
        CloseHandle(t);
        return;
    }
    std::unique_ptr<Job> back(raw);  // no thread: answered at once as not reached
    Done& d = done();
    AcquireSRWLockExclusive(&d.lock);
    d.jobs.push_back(std::move(back));
    ReleaseSRWLockExclusive(&d.lock);
}

void pump() {
    Done& d = done();
    std::deque<std::unique_ptr<Job>> ready;
    AcquireSRWLockExclusive(&d.lock);
    ready.swap(d.jobs);
    ReleaseSRWLockExclusive(&d.lock);
    for (std::unique_ptr<Job>& j : ready) {
        InterlockedDecrement(&d.inFlight);
        LOG_INFO("site: %s answered %d", pathOnly(j->request.path).c_str(), j->status);
        if (j->answer) j->answer(j->status, j->body);
        if (!j->token.empty()) SecureZeroMemory(j->token.data(), j->token.size());
        if (!j->request.body.empty())  // a password may be in it
            SecureZeroMemory(j->request.body.data(), j->request.body.size());
    }
}

bool busy() { return done().inFlight > 0; }

}  // namespace uagfx::win::sitehttp
