#include "app/pack_fetch.hpp"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <functional>
#include <vector>

#include "app/text.hpp"
#include "core/pack_download.hpp"
#include "core/texture_pack.hpp"

namespace uagfx::app {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
FetchState g_state;
HANDLE g_thread = nullptr;
volatile LONG g_stop = 0;
std::wstring g_dir;
std::string g_url;

void update(const std::function<void(FetchState&)>& f) {
    AcquireSRWLockExclusive(&g_lock);
    f(g_state);
    ReleaseSRWLockExclusive(&g_lock);
}

// One HTTPS connection to the pack's host: GETs of byte ranges.
class Remote {
public:
    bool open(const std::string& url, std::string& error) {
        URL_COMPONENTSW c{};
        c.dwStructSize = sizeof c;
        wchar_t host[256], path[1024];
        c.lpszHostName = host;
        c.dwHostNameLength = 256;
        c.lpszUrlPath = path;
        c.dwUrlPathLength = 1024;
        const std::wstring w = wide(url);
        if (!WinHttpCrackUrl(w.c_str(), 0, 0, &c) || c.nScheme != INTERNET_SCHEME_HTTPS) {
            error = "not an https address: " + url;
            return false;
        }
        path_ = path;
        session_ = WinHttpOpen(L"uaRO Graphics Settings", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0);
        if (session_) WinHttpSetTimeouts(session_, 15000, 15000, 60000, 60000);
        connect_ = session_ ? WinHttpConnect(session_, host, c.nPort, 0) : nullptr;
        if (!connect_) {
            error = "no connection (error " + std::to_string(GetLastError()) + ")";
            return false;
        }
        return true;
    }
    ~Remote() {
        if (connect_) WinHttpCloseHandle(connect_);
        if (session_) WinHttpCloseHandle(session_);
    }
    // The file's size (a HEAD).
    bool size(uint64_t& out, std::string& error) {
        HINTERNET r = request(L"HEAD", L"");
        DWORD status = 0;
        wchar_t length[32] = {};
        DWORD n = sizeof length, s = sizeof status;
        const bool ok = r && WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                                 WINHTTP_HEADER_NAME_BY_INDEX, &status, &s, WINHTTP_NO_HEADER_INDEX) &&
                        status == 200 &&
                        WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, length, &n,
                                            WINHTTP_NO_HEADER_INDEX);
        if (r) WinHttpCloseHandle(r);
        if (!ok) {
            error = status ? "the site answered " + std::to_string(status) + " for the pack"
                           : "no answer from the site";
            return false;
        }
        out = _wcstoui64(length, nullptr, 10);
        return out > 22;
    }
    // Bytes [at, at + n) into out; progress(n) as they come; false on a stop or a failure.
    bool get(uint64_t at, size_t n, uint8_t* out, const std::function<void(size_t)>& progress, std::string& error) {
        const std::wstring range = L"Range: bytes=" + std::to_wstring(at) + L"-" + std::to_wstring(at + n - 1);
        HINTERNET r = request(L"GET", range);
        DWORD status = 0, s = sizeof status;
        if (!r || !WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                       WINHTTP_HEADER_NAME_BY_INDEX, &status, &s, WINHTTP_NO_HEADER_INDEX) ||
            status != 206) {
            if (r) WinHttpCloseHandle(r);
            error = status ? "the site answered " + std::to_string(status) + " for a part of the pack"
                           : "the connection was lost (error " + std::to_string(GetLastError()) + ")";
            return false;
        }
        size_t got = 0;
        while (got < n) {
            if (g_stop) break;
            DWORD read = 0;
            if (!WinHttpReadData(r, out + got, DWORD(std::min<size_t>(n - got, 1u << 20)), &read) || !read) break;
            got += read;
            if (progress) progress(read);
        }
        WinHttpCloseHandle(r);
        if (got != n && !g_stop) error = "the connection was lost in the middle of a part";
        return got == n;
    }

private:
    HINTERNET request(const wchar_t* verb, const std::wstring& header) {
        HINTERNET r = WinHttpOpenRequest(connect_, verb, path_.c_str(), nullptr, WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!r) return nullptr;
        if (!WinHttpSendRequest(r, header.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : header.c_str(),
                                header.empty() ? 0 : DWORD(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(r, nullptr)) {
            WinHttpCloseHandle(r);
            return nullptr;
        }
        return r;
    }
    HINTERNET session_ = nullptr, connect_ = nullptr;
    std::wstring path_;
};

void fail(const std::string& why) {
    update([&](FetchState& s) {
        s.kind = g_stop ? FetchState::Stopped : FetchState::Failed;
        s.error = why;
    });
}

DWORD WINAPI run(void*) {
    Remote remote;
    std::string error;
    uint64_t size = 0;
    if (!remote.open(g_url, error) || !remote.size(size, error)) {
        fail(error);
        return 0;
    }
    std::vector<pack::ZipEntry> entries;
    const pack::ReadAt read = [&](uint64_t at, uint8_t* out, size_t n) { return remote.get(at, n, out, {}, error); };
    if (!pack::centralDirectoryOf(size, read, entries, error)) {
        fail("the pack on the site: " + error);
        return 0;
    }
    // One writer of the pack at a time (its builder, a game making pictures, this).
    HANDLE mutex = CreateMutexA(nullptr, FALSE, pack::kWriterMutex);
    if (!mutex || WaitForSingleObject(mutex, 0) == WAIT_TIMEOUT) {
        if (mutex) CloseHandle(mutex);
        fail("the pack is being written by the game or its builder now - close them and try again");
        return 0;
    }
    const std::string path = ansi(g_dir + L"\\uaro-textures.zip");
    std::string localIndex;
    {
        pack::Reader r;
        std::vector<uint8_t> bytes;
        if (r.open(path, error) && r.read(pack::kIndexName, bytes)) localIndex.assign(bytes.begin(), bytes.end());
    }
    pack::Writer w;
    if (!w.open(path, true, error)) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        fail(error);
        return 0;
    }
    // The site's index, merged with the local one at every checkpoint.
    std::string siteIndex;
    for (const pack::ZipEntry& e : entries)
        if (e.name == pack::kIndexName && e.method == 0) {
            const size_t n = size_t(std::min<uint64_t>(size - e.header, 30 + e.name.size() + 1024 + e.compressed));
            std::vector<uint8_t> b(n);
            const uint8_t* data = nullptr;
            if (remote.get(e.header, n, b.data(), {}, error) && pack::entryData(b.data(), n, e.header, e, data, error))
                siteIndex.assign(reinterpret_cast<const char*>(data), size_t(e.compressed));
        }
    const std::vector<pack::Span> spans =
        pack::downloadSpans(entries, [&](const std::string& n) { return w.has(n); }, size);
    uint64_t total = 0;
    uint32_t pictures = 0;
    for (const pack::Span& s : spans) {
        total += s.to - s.from;
        pictures += uint32_t(s.entries.size());
    }
    const uint32_t already = uint32_t(w.count());
    update([&](FetchState& s) {
        s.kind = FetchState::Fetching;
        s.total = total;
        s.pictures = pictures;
        s.already = already;
    });
    const ULONGLONG start = GetTickCount64();
    ULONGLONG saved = start;
    uint64_t done = 0;
    bool ok = true;
    std::vector<uint8_t> buffer;
    for (const pack::Span& span : spans) {
        if (g_stop) break;
        buffer.resize(size_t(span.to - span.from));
        const auto progress = [&](size_t n) {
            done += n;
            const double seconds = double(GetTickCount64() - start) / 1000.0;
            update([&](FetchState& s) {
                s.done = done;
                s.bytesPerSecond = seconds > 0.5 ? double(done) / seconds : 0;
            });
        };
        if (!remote.get(span.from, buffer.size(), buffer.data(), progress, error)) {
            ok = false;
            break;
        }
        uint32_t added = 0;
        for (size_t i : span.entries) {
            const uint8_t* data = nullptr;
            if (!pack::entryData(buffer.data(), buffer.size(), span.from, entries[i], data, error) ||
                !w.add(entries[i].name, data, size_t(entries[i].compressed), error)) {
                ok = false;
                break;
            }
            ++added;
        }
        update([&](FetchState& s) { s.fetched += added; });
        if (!ok) break;
        if (GetTickCount64() - saved > 30000) {  // a whole ZIP every half a minute: a cut keeps what came
            w.checkpoint(pack::mergedIndex(localIndex, siteIndex), error);
            saved = GetTickCount64();
        }
    }
    std::string closing;
    w.close(pack::mergedIndex(localIndex, siteIndex), closing);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    if (!ok || g_stop) {
        fail(ok ? "" : error);
        return 0;
    }
    update([&](FetchState& s) { s.kind = closing.empty() ? FetchState::Done : FetchState::Failed; s.error = closing; });
    return 0;
}

}  // namespace

void startFetch(const std::wstring& gameDir, const std::string& url) {
    if (fetchRunning()) return;
    if (g_thread) CloseHandle(g_thread);
    g_dir = gameDir;
    g_url = url;
    g_stop = 0;
    update([](FetchState& s) {
        s = FetchState();
        s.kind = FetchState::Listing;
    });
    g_thread = CreateThread(nullptr, 0, run, nullptr, 0, nullptr);
    if (!g_thread) fail("cannot start a thread");
}

void stopFetch() { InterlockedExchange(&g_stop, 1); }

FetchState fetchState() {
    AcquireSRWLockShared(&g_lock);
    const FetchState s = g_state;
    ReleaseSRWLockShared(&g_lock);
    return s;
}

bool fetchRunning() {
    const FetchState::Kind k = fetchState().kind;
    return k == FetchState::Listing || k == FetchState::Fetching;
}

}  // namespace uagfx::app
