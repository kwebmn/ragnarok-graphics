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

// 2.13: `body` - a POST of JSON instead of the GET (with the timeouts of the reports' POST since 0.51)
bool open(Get& g, const std::string& path, uint64_t range, std::string& error, const std::string* body = nullptr) {
    g.session = WinHttpOpen(L"uaRO Graphics", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
    if (g.session && body) WinHttpSetTimeouts(g.session, 10000, 10000, 30000, 30000);
    g.connect = g.session ? WinHttpConnect(g.session, wide(update::kSite).c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0)
                          : nullptr;
    const std::wstring wpath = wide(path);
    g.request = g.connect ? WinHttpOpenRequest(g.connect, body ? L"POST" : L"GET", wpath.c_str(), nullptr,
                                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                          : nullptr;
    const std::wstring header = body    ? L"Content-Type: application/json\r\nAccept: application/json\r\n"
                                : range ? L"Range: bytes=" + std::to_wstring(range) + L"-"
                                        : L"";
    void* data = body ? const_cast<char*>(body->data()) : WINHTTP_NO_REQUEST_DATA;
    const DWORD length = body ? DWORD(body->size()) : 0;
    DWORD size = sizeof g.status;
    const bool sent = g.request &&
                      WinHttpSendRequest(g.request, header.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : header.c_str(),
                                         header.empty() ? 0 : DWORD(-1L), data, length, length, 0) &&
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

// A file of kFiles with a number after its name: no proxy or cache on the way keeps an older one (nginx ignores it).
std::string filePath(const std::string& file) {
    return std::string(update::kFiles) + file + "?" + std::to_string(GetTickCount());
}

// false when the answer is longer than `most`
bool readAll(Get& g, std::string& out, size_t most) {
    DWORD n = 0;
    while (WinHttpQueryDataAvailable(g.request, &n) && n) {
        if (out.size() + n > most) return false;
        std::string part(n, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(g.request, &part[0], n, &read) || !read) break;
        out.append(part, 0, read);
    }
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
        if (!open(g, filePath(file), have, error)) return false;
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
    return fetchPath(filePath(file), out, error);
}

bool fetchPath(const std::string& path, std::string& out, std::string& error, size_t most) {
    out.clear();
    Get g;
    if (!open(g, path, 0, error)) return false;
    if (g.status != 200) {
        error = std::string(update::kSite) + " answered " + std::to_string(g.status) + " for " + path;
        return false;
    }
    if (!readAll(g, out, most)) {
        error = std::string(update::kSite) + " answered more than " + std::to_string(most) + " bytes for " + path;
        return false;
    }
    return true;
}

bool postPath(const std::string& path, const std::string& body, long& status, std::string& answer,
              std::string& error) {
    answer.clear();
    status = 0;
    Get g;
    if (!open(g, path, 0, error, &body)) return false;
    status = long(g.status);
    readAll(g, answer, 64u << 10);
    return true;
}

}  // namespace uagfx::app
