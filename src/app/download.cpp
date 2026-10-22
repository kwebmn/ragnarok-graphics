#include "app/download.hpp"

#include <windows.h>
#include <winhttp.h>

#include "app/text.hpp"
#include "core/update_check.hpp"

namespace uagfx::app {
namespace {

// One GET of https://kSite kFiles `file` (with `range` from that byte on when not 0): the open request and its
// status, or nullptr and why. The caller closes the three handles.
struct Get {
    HINTERNET session = nullptr, connect = nullptr, request = nullptr;
    DWORD status = 0;
    uint64_t length = 0;  // Content-Length, 0 unknown
    ~Get() {
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
    }
};

bool open(Get& g, const std::string& file, uint64_t range, std::string& error) {
    g.session = WinHttpOpen(L"uaRO Graphics", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
    g.connect = g.session ? WinHttpConnect(g.session, wide(update::kSite).c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0)
                          : nullptr;
    // A number after the name: no proxy or cache on the way keeps an older update.ini (nginx ignores it).
    const std::wstring path = wide(std::string(update::kFiles) + file + "?" + std::to_string(GetTickCount()));
    g.request = g.connect ? WinHttpOpenRequest(g.connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                          : nullptr;
    const std::wstring header = range ? L"Range: bytes=" + std::to_wstring(range) + L"-" : L"";
    DWORD size = sizeof g.status;
    const bool sent = g.request &&
                      WinHttpSendRequest(g.request, range ? header.c_str() : WINHTTP_NO_ADDITIONAL_HEADERS,
                                         range ? DWORD(-1L) : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                      WinHttpReceiveResponse(g.request, nullptr) &&
                      WinHttpQueryHeaders(g.request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                          WINHTTP_HEADER_NAME_BY_INDEX, &g.status, &size, WINHTTP_NO_HEADER_INDEX);
    if (!sent) {
        error = "no answer from " + std::string(update::kSite) + " (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    wchar_t text[32] = {};
    DWORD bytes = sizeof text;
    if (WinHttpQueryHeaders(g.request, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, text, &bytes,
                            WINHTTP_NO_HEADER_INDEX))
        g.length = _wcstoui64(text, nullptr, 10);
    return true;
}

uint64_t sizeOf(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a)) return 0;
    return uint64_t(a.nFileSizeHigh) << 32 | a.nFileSizeLow;
}

}  // namespace

bool download(const std::string& file, const std::wstring& to,
              const std::function<bool(uint64_t done, uint64_t total)>& progress, std::string& error) {
    const std::wstring part = to + L".part";
    for (int attempt = 0; attempt < 2; ++attempt) {
        uint64_t have = sizeOf(part);
        Get g;
        if (!open(g, file, have, error)) return false;
        if (have && g.status == 416) {  // the .part is the whole file or more: start over once
            DeleteFileW(part.c_str());
            continue;
        }
        if (g.status != 200 && g.status != 206) {
            error = std::string(update::kSite) + " answered " + std::to_string(g.status) + " for " + file;
            return false;
        }
        if (g.status == 200) have = 0;  // the whole file again
        HANDLE f = CreateFileW(part.c_str(), GENERIC_WRITE, 0, nullptr, have ? OPEN_ALWAYS : CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) {
            error = "cannot write " + utf8(part) + " (error " + std::to_string(GetLastError()) + ")";
            return false;
        }
        LARGE_INTEGER end{};
        SetFilePointerEx(f, end, nullptr, FILE_END);
        const uint64_t total = have + g.length;
        uint64_t done = have;
        bool ok = progress(done, total);
        DWORD n = 0;
        std::string buffer;
        while (ok && WinHttpQueryDataAvailable(g.request, &n) && n) {
            buffer.resize(n);
            DWORD read = 0, wrote = 0;
            if (!WinHttpReadData(g.request, &buffer[0], n, &read) || !read) break;
            if (!WriteFile(f, buffer.data(), read, &wrote, nullptr) || wrote != read) {
                error = "cannot write " + utf8(part);
                ok = false;
                break;
            }
            done += read;
            ok = progress(done, total);
            if (!ok && error.empty()) error = "stopped";
        }
        CloseHandle(f);
        if (!ok) return false;
        if (g.length && done != total) {
            error = "the connection broke at " + std::to_string(done) + " of " + std::to_string(total) + " bytes";
            return false;  // the .part stays: the next try continues it
        }
        if (!MoveFileExW(part.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            error = "cannot write " + utf8(to);
            return false;
        }
        return true;
    }
    error = std::string(update::kSite) + " refused to continue " + file;
    return false;
}

bool fetch(const std::string& file, std::string& out, std::string& error) {
    out.clear();
    Get g;
    if (!open(g, file, 0, error)) return false;
    if (g.status != 200) {
        error = std::string(update::kSite) + " answered " + std::to_string(g.status) + " for " + file;
        return false;
    }
    DWORD n = 0;
    while (WinHttpQueryDataAvailable(g.request, &n) && n && out.size() < (64u << 10)) {
        std::string part(n, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(g.request, &part[0], n, &read) || !read) break;
        out.append(part, 0, read);
    }
    return true;
}

}  // namespace uagfx::app
